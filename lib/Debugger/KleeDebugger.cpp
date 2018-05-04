#include <assert.h>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <vector>

#include "../Core/Searcher.h"
#include "../Core/StatsTracker.h"
#include "../Core/Executor.h"
#include "../Core/Memory.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueSymbolTable.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Statistics.h"

namespace klee {

namespace {
std::string getSourceLine(const std::string &file, unsigned line) {
    std::string res("");
    std::ifstream in;
    in.open(file.c_str());
    if (!in.fail()) {
        for (unsigned i = 1; i <= line; ++i) {
            std::getline(in, res);
            if (in.eof()) {
                res = "";
                break;
            }
        }
        in.close();
    }
    return res;
}

std::string getFileFromPath(const std::string &fullPath) {
    size_t i = fullPath.find_last_of('/');
    if (i == std::string::npos) {
        return fullPath;
    }
    return fullPath.substr(i + 1);
}

const char *MSG_INTERRUPTED
    = "KLEE: ctrl-c detected, execution interrupted> ";
const char *MSG_SELECT_STATE
    = "Select a state to continue by entering state number:";
const int itemsPerPage = 4;
}

extern "C" void set_halt_execution(bool);
extern "C" void set_interrupted(bool);
extern "C" bool klee_interrupted();
using namespace debugcommands;

void (KDebugger::*(KDebugger::processors)[])(std::string &) = {
    &KDebugger::processContinue,
    &KDebugger::processRun,
    &KDebugger::processStep,
    &KDebugger::processQuit,
    &KDebugger::processBreakpoint,
    &KDebugger::processDelete,
    &KDebugger::processPrint,
    &KDebugger::processSet,
    &KDebugger::processInfo,
    &KDebugger::processState,
    &KDebugger::processTerminate,
    &KDebugger::generateConcreteInput,
    &KDebugger::processHelp,
};

KDebugger::KDebugger() :
    prompt(this),
    searcher(0),
    statsTracker(0),
    breakpoints(),
    breakTable(),
    step(true) {}

void KDebugger::preprocess() {
    if (klee_interrupted()) {
        step = false;
        set_interrupted(false);
        set_halt_execution(false);
        prompt.show(MSG_INTERRUPTED);
    }

    if (step || searcher->newStates()) {
        // Check if execution branched.
        if (searcher->newStates()) {
            unsigned int newStates = searcher->newStates();
            auto states = searcher->getStates();
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "Execution branched\n";
            if (step || stopUponBranching) {
                unsigned int cnt = 0;
                for (auto it = states.end() - newStates - 1; it != states.end(); ++it) {
                    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
                    llvm::outs() << "Enter " << ++cnt << " to select ";
                    printState(*it);
                    llvm::outs() << "\n";
                }
                prompt.show(MSG_SELECT_STATE);
                showPromptAtInstruction(searcher->currentState()->pc);
                return;
            } else {
                for (auto it = states.end() - newStates - 1; it != states.end(); ++it) {
                    printState(*it);
                    llvm::outs() << "\n";
                }
                llvm::outs().changeColor(llvm::raw_ostream::CYAN);
                llvm::outs() << "Continuing execution from state @" << *(states.end() - 1) << "\n";
                llvm::outs().changeColor(llvm::raw_ostream::WHITE);
                set_halt_execution(false);
                checkBreakpoint(*searcher->currentState());
            }
        } else {
            step = false;
            showPromptAtInstruction(searcher->currentState()->pc);
            return;
        }
    } else {
        checkBreakpoint(*searcher->currentState());
    }
}

void KDebugger::showPromptAtInstruction(const KInstruction *ki) {
    std::string file = getFileFromPath(ki->info->file);
    if (file == "") {
        prompt.show("KLEE debugger, type h for usage> ");
        return;
    }
    std::string msg = file + ", line " + std::to_string(ki->info->line) + "> ";
    prompt.show(msg.c_str());
}

void KDebugger::checkBreakpoint(ExecutionState &state) {
    if (state.pc->info->file == state.prevPC->info->file
        && state.pc->info->line == state.prevPC->info->line) {
        return;
    }
    if (breakpoints.empty() || !state.pc) {
        return;
    }
    auto ki = state.pc;
    std::string fileLine = getFileFromPath(state.pc->info->file) +
                           std::to_string(state.pc->info->line);
    if (breakTable[fileLine]) {
        std::string file = getFileFromPath(ki->info->file);
        std::string msg = "Breakpoint hit: " + file + ", line " + std::to_string(ki->info->line) + "> ";
        prompt.show(msg.c_str());
    }
}

void KDebugger::handleCommand(std::vector<std::string> &input, std::string &msg) {
    selected = CommandType::none;
    if ((step || stopUponBranching) && searcher->newStates()) {
        auto res = clipp::parse(input, branchSelection);
        if (res) {
            selectBranch(stateIdx, msg);
        }
        return;
    }
    for (auto &cli : cmds) {
        std::vector<std::string> v;
        v.push_back(input[0]);
        auto res = clipp::parse(v, one_of(cli[0].as_param()));
        if (selected != CommandType::none) {
            res = clipp::parse(input, cli);
            if (res && extraArgs.empty()) {
                (this->*(KDebugger::processors[(int)selected]))(msg);
                return;
            } else {
                for (auto const & arg: extraArgs) {
                    llvm::outs() << "Unsupported arguments: " << arg << "\n";
                }
                extraArgs.clear();
                break;
            }
        }
    }
    llvm::outs() << "Invalid input, type h to see usage.\n";
}

void KDebugger::processContinue(std::string &) {
    llvm::outs() << "Continue execution..\n";
    prompt.breakFromLoop();
}

void KDebugger::processRun(std::string &) {
    llvm::outs() << "Continue execution to end of program..\n";
    breakpoints.clear();
    prompt.breakFromLoop();
}

void KDebugger::processStep(std::string &) {
    llvm::outs() << "Stepping..\n";
    step = true;
    auto state = searcher->currentState();
    printCode(state);
    prompt.breakFromLoop();
}

void KDebugger::processQuit(std::string &) {
    set_halt_execution(true);
    prompt.breakFromLoop();
}

void KDebugger::processBreakpoint(std::string &) {
    std::regex r("(.*\\.\\w*)\\:([0-9]+)");
    std::regex breakpointRegex("(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    if (std::regex_search(bpString.c_str(), matches, r)) {
        Breakpoint nbp(matches.str(1), (unsigned)stoi(matches.str(2)));
        auto it = std::find_if(breakpoints.begin(), breakpoints.end(), [&nbp](const Breakpoint &bp) {
            return bp.file == nbp.file && bp.line == nbp.line;
        });

        if (it == breakpoints.end()) {
            breakpoints.push_back(nbp);
            breakTable[nbp.file + std::to_string(nbp.line)] = true;
            Breakpoint::cnt++;
            llvm::outs() << "New breakpoint set\n";
        } else {
            llvm::outs() << "Breakpoint already exists\n";
        }
    } else {
        llvm::outs() << "Invalid breakpoint format\n";
    }
}

void KDebugger::processDelete(std::string &) {
    unsigned sz = breakpointNumbers.size();
    if (sz) {
        breakpoints.erase(std::remove_if(breakpoints.begin(), breakpoints.end(), [&](const Breakpoint &bp){
            auto it = std::find(breakpointNumbers.begin(), breakpointNumbers.end(), bp.idx);
            if (it != breakpointNumbers.end()) {
                std::string fileLine = bp.file + std::to_string(bp.line);
                breakTable.erase(fileLine);
                breakpointNumbers.erase(it);
                return true;
            }
            return false;
        }), breakpoints.end());
        llvm::outs() << (sz - breakpointNumbers.size()) << " breakpoints successfully removed.\n";
        if (breakpointNumbers.size()) {
            llvm::outs() << "No breakpoints with the following number(s):\n";
            for (auto num : breakpointNumbers) {
                llvm::outs() << num << " ";
            }
            llvm::outs() << "\n Type info break to list all breakpoints.\n";
        }
    } else {
        for (auto bp : breakpoints) {
            std::string fileLine = bp.file + std::to_string(bp.line);
            breakTable.erase(fileLine);
        }
        breakpoints.clear();
        llvm::outs() << "All breakpoints removed.\n";
    }
}

void KDebugger::processPrint(std::string &) {
    llvm::outs() << "Printing variable: " << var << "\n";
    auto stackFrame = searcher->currentState()->stack.back();
    auto func = stackFrame.kf->function;

    auto mo = getMemoryObjectBySymbol(var);
    if (mo) {
        auto os = searcher->currentState()->addressSpace.findObject(mo);
        if (os) {
            os->print();
            return;
        }
    }

    auto &st = func->getValueSymbolTable();
    auto value = st.lookup(llvm::StringRef(var.c_str()));

    if (value) {
        value->dump();
        return;
    }

    llvm::outs() << "Unable to print variable information\n";
}

void KDebugger::processSet(std::string &) {
    llvm::outs() << "Setting variable: " << var << "\n";
    auto state = searcher->currentState();

    auto mo = getMemoryObjectBySymbol(var);
    if (mo) {
        executor->executeMakeSymbolic(*state, mo, var);
        return;
    }

    llvm::outs() << "Unable to find variable " << var << "\n";
}

void KDebugger::processInfo(std::string &) {
    auto state = searcher->currentState();
    switch (infoOpt) {
        case InfoOpt::stack: printStack(state); break;
        case InfoOpt::constraints: printConstraints(state); break;
        case InfoOpt::breakpoints: printBreakpoints(); break;
        case InfoOpt::states: printAllStates(); break;
        case InfoOpt::statistics: statsTracker->printStats(llvm::outs()); break;
        case InfoOpt::state: printState(state); break;
        default:
            llvm::outs() << "Invalid info option, type \"h\" for help.\n";
    }
    infoOpt = InfoOpt::state;
}

void KDebugger::processState(std::string &msg) {
    if (stateAddrHex.size()) {
        char *res = 0;
        unsigned long long addr = strtoul(stateAddrHex.c_str(), &res, 16);
        if (*res != 0) {
            llvm::outs() << "Please enter a valid address (hex number) \n";
            return;
        } else {
            searcher->setStateAtAddr(addr);
        }
        stateAddrHex = "";
    } else {
        searcher->nextIter();
    }
    auto *state = searcher->currentState();
    llvm::outs() <<  "Moved to state @";
    llvm::outs().write_hex((unsigned long long)state);
    llvm::outs() << "\n";
    msg = getFileFromPath(state->pc->info->file) + ", line " +
          std::to_string(state->pc->info->line) + "> ";
}

void KDebugger::selectBranch(int idx, std::string &msg) {
    unsigned int newStates = searcher->newStates();
    if (idx < 1 || idx > (1 + (int)newStates)) {
        llvm::outs() << "Please enter a number in the range [1, "
            << (1 + newStates) << "]\n";
        return;
    }

    searcher->selectNewState(idx);
    auto state = searcher->currentState();
    llvm::outs() << "You selected state @" << state << ".\n";
    stopUponBranching = false;
    prompt.breakFromLoop();
}

void KDebugger::processTerminate(std::string &) {
    llvm::Twine msg("");
    switch (termOpt) {
        case TerminateOpt::current:
            executor->terminateStateEarly(*searcher->currentState(), msg);
            executor->updateStates(nullptr);
            if (searcher->getStates().size() == 0) {
                prompt.breakFromLoop();
            }
            break;
        case TerminateOpt::other:
            for (auto state : searcher->getStates()) {
                if (state != searcher->currentState()) {
                    executor->terminateStateEarly(*state, msg);
                }
            }
            executor->updateStates(nullptr);
            break;
        default:
            llvm::outs() << "Invalid terminate option\n";
    }
    termOpt = TerminateOpt::current;
}

void KDebugger::generateConcreteInput(std::string &) {

    printConstraints(searcher->currentState());

    std::vector<std::pair<std::string, std::vector<unsigned char>>> out;
    bool success = executor->getSymbolicSolution(*searcher->currentState(), out);

    if (!success) {
        llvm::outs().changeColor(llvm::raw_ostream::RED);
        llvm::outs() << "No satisfying input values found\n";
        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        return;
    }

    if (!out.size()) {
        return;
    }

    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "\nConcrete input value(s) that will reach this path:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);

    for (auto &p : out) {
      llvm::outs() << p.first << " (size " << p.second.size() << "): ";
      for (auto c : p.second) {
        llvm::outs() << llvm::format("\\0x%02x", int(c));
      }
      llvm::outs() << "\n";
    }
}

void KDebugger::processHelp(std::string &) {
    using namespace clipp;
    auto fmt = doc_formatting{}.start_column(6).doc_column(26).paragraph_spacing(0);;
    llvm::outs () <<"USAGE:\n" << usage_lines(cmdParser) << "\n\n";
    llvm::outs() << "DOCUMENTATION:\n" << documentation(cmdParser, fmt);;
}

void KDebugger::printBreakpoints() {
    llvm::outs() << breakpoints.size() << " breakpoints set\n";
    const unsigned fileCol = 9;
    const unsigned lineCol = 36;
    llvm::formatted_raw_ostream fo(llvm::outs());
    fo << "Num";
    fo.PadToColumn(fileCol);
    fo << "File";
    fo.PadToColumn(lineCol);
    fo << "Line\n";
    for (auto & bp : breakpoints) {
        fo << bp.idx;
        fo.PadToColumn(fileCol);
        fo << bp.file;
        fo.PadToColumn(lineCol);
        fo << bp.line << "\n";
    }
}

void KDebugger::printAllStates() {
    auto states = searcher->getStates();
    int total = states.size();
    llvm::outs().changeColor(llvm::raw_ostream::GREEN);
    llvm::outs() << "Total number of states: " << total;
    llvm::outs() << "\n";

    for (int i = 0; i < std::min(itemsPerPage, total); ++i) {
        if (states[i] == searcher->currentState()) {
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "current state --> ";
            llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        }
        printState(states[i]);
        llvm::outs() << "\n";
    }

    if (total <= itemsPerPage) return;
    int page = 0;
    const char* msg = "n(p) for next(previous) page, q to quit>";
    char *cmd;
    while ((cmd = linenoise(msg)) != NULL) {
        std::string str(cmd);
        if (str[0] != '\0') {
            if (str == "q") break;
            if (str == "n" || str == "p") {
                page = str == "n" ? std::min(page + 1, total / itemsPerPage)
                                  : std::max(page - 1, 0);
                for (int i = itemsPerPage * page; i < total && i < itemsPerPage * (page + 1); ++i) {
                    if (states[i] == searcher->currentState()) {
                        llvm::outs().changeColor(llvm::raw_ostream::GREEN);
                        llvm::outs() << "current state --> ";
                        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
                    }
                    printState(states[i]);
                    llvm::outs() << "\n";
                }
            }
            linenoiseFree(cmd);
        }
    }
    linenoiseFree(cmd);
}

void KDebugger::printState(ExecutionState *state) {
    assert(state);
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "state @" << state << ", ";
    printConstraints(state);
    llvm::outs() << "\n";
    printCode(state);
}

void KDebugger::printCode(ExecutionState *state) {
    assert(state);
    auto info = state->pc->info;
    if (info->file == "") {
        return;
    }
    std::string line = getSourceLine(info->file, info->line);
    size_t first = line.find_first_not_of(' ');
    std::string trimmed = line.substr(first, line.size() - first + 1);
    llvm::outs() << "file: " << info->file << "\n";
    llvm::outs() << "line: " << info->line << "\n";
    llvm::outs() << "source: " << trimmed << "\n";
    state->pc->printFileLine();
    llvm::outs() << "assembly:";
    state->pc->inst->print(llvm::outs());
    llvm::outs() << "\n";
}

void KDebugger::printStack(ExecutionState *state) {
    assert(state);
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(llvm::outs());
}

void KDebugger::printConstraints(ExecutionState *state) {
    assert(state);
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "constraints for state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), state->constraints);
}

const MemoryObject *KDebugger::getMemoryObjectBySymbol(std::string &var) {
    auto stackFrame = searcher->currentState()->stack.back();
    auto func = stackFrame.kf->function;
    const auto &args = func->getArgumentList();
    unsigned int idx = 1; // 0 is retval

    for (auto &arg : args) {
        if (arg.getName() == var && idx < stackFrame.allocas.size()) {
            return stackFrame.allocas[idx];
        }
        idx++;
    }

    for (auto mo : stackFrame.allocas) {
        if (mo->name == var)
            return mo;
    }

    return NULL;
}

}