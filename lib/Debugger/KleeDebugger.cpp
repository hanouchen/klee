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
#include "llvm/DebugInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueSymbolTable.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Debugger/DebugSymbolTable.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Module/KInstruction.h"
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

std::string getSourceLines(const std::string &file, int from, int to) {
    std::string codeBlock = {};
    std::string line = {};
    std::ifstream in;
    in.open(file.c_str());
    if (!in.fail()) {
        for (int i = 1; i <= to; ++i) {
            std::getline(in, line);
            if (i >= from) codeBlock += std::to_string(i) + "\t" + line + "\n";
            if (in.eof()) {
                line = "";
                break;
            }
        }
        in.close();
    }
    return codeBlock;
}

std::string getFileFromPath(const std::string &fullPath) {
    size_t i = fullPath.find_last_of('/');
    if (i == std::string::npos) {
        return fullPath;
    }
    return fullPath.substr(i + 1);
}

const char *MSG_INTERRUPTED
    = "(kleedb, interrupted) ";
const char *MSG_SELECT_STATE
    = "(Select a state to continue) ";
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
    &KDebugger::processStepInstruction,
    &KDebugger::processQuit,
    &KDebugger::processBreakpoint,
    &KDebugger::processBreakpoint,
    &KDebugger::processDelete,
    &KDebugger::processPrint,
    &KDebugger::processPrintRegister,
    &KDebugger::processCodeListing,
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
    killTable(),
    step(true),
    stepi(true) {}

void KDebugger::preprocess() {
    if (klee_interrupted()) {
        step = false;
        stepi = false;
        set_interrupted(false);
        set_halt_execution(false);
        prompt.show(MSG_INTERRUPTED);
    }

    if (step || stepi || searcher->newStates()) {
        // Check if execution branched.
        if (searcher->newStates()) {
            unsigned int newStates = searcher->newStates();
            auto states = searcher->getStates();
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "Execution branched\n";
            if (step || stepi || stopUponBranching) {
                unsigned int cnt = 0;
                for (auto it = states.end() - newStates - 1; it != states.end(); ++it) {
                    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
                    llvm::outs() << "Enter " << ++cnt << " to select ";
                    printState(*it);
                    llvm::outs() << "\n";
                }
                prompt.show(MSG_SELECT_STATE);
                prompt.show();
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
            auto pc = searcher->currentState()->pc;
            auto prev = searcher->currentState()->prevPC;

			if (!stepi && pc->info->file == "") {
				return;
			}

            if (stepi || !prev || pc->info->file != prev->info->file || pc->info->line != prev->info->line) {
                stepi = false;
                step = false;
                prompt.show();
                return;
            }
        }
    } else {
        checkBreakpoint(*searcher->currentState());
    }
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
    std::string file = getFileFromPath(ki->info->file);
	if (file == "") return;
    std::string line = std::to_string(ki->info->line);
    std::string fileLine = getFileFromPath(state.pc->info->file) +
                           std::to_string(state.pc->info->line);
    if (killTable[fileLine]) {
        std::string str;
		state.pc->inst->dump();
        llvm::outs().changeColor(llvm::raw_ostream::RED);
        llvm::outs() << "\nKillpoint " << killTable[fileLine] << ", at "  << file << ":" << line << "\n";
        llvm::outs() << "Terminating current state @" << &state << "\n";
        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        termOpt = TerminateOpt::current;
        processTerminate(str);
        prompt.breakFromLoop();
        return;
    }
    int &bp = breakTable[fileLine];
    if (bp > 0) {
        ki->breakPoint = true;
        bp *= -1;
    }
    if (bp < 0 && ki->breakPoint) {
        llvm::outs() << "\nBreakpoint " << (-bp) << ", at "  << file << ":" << line << "\n";
        printCode(searcher->currentState(), true);
        prompt.show();
    }
}

void KDebugger::handleCommand(std::vector<std::string> &input, std::string &msg) {
    selected = CommandType::none;
    if ((step || stepi || stopUponBranching) && searcher->newStates()) {
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
    step = true;
    auto state = searcher->currentState();
    printCode(state, false);
    prompt.breakFromLoop();
}

void KDebugger::processStepInstruction(std::string &) {
    stepi = true;
    auto state = searcher->currentState();
    printCode(state, true);
    prompt.breakFromLoop();
}

void KDebugger::processQuit(std::string &) {
    set_halt_execution(true);
    prompt.breakFromLoop();
}

void KDebugger::processBreakpoint(std::string &) {
    PType type = selected == CommandType::breakpoint ? PType::breakpoint
                                                     : PType::killpoint;

    std::regex r("(.*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    std::string file = {};
    unsigned int line = 0;
    if (std::regex_search(bpString.c_str(), matches, r)) {
        file = matches.str(1);
        line = (unsigned)stoi(matches.str(2));
    } else {
        auto info = executor->kmodule->infos->getFunctionInfoByName(bpString);
        if (info.file == "") {
            llvm::outs() << "No function named " << bpString  << "\n";
            return;
        }
        file = getFileFromPath(info.file);
        line = info.line;
    }

    Breakpoint nbp(type, file, line);
    auto it = std::find_if(breakpoints.begin(), breakpoints.end(), [&nbp](const Breakpoint &bp) {
        return bp.file == nbp.file && bp.line == nbp.line;
    });

    if (it == breakpoints.end()) {
        breakpoints.push_back(nbp);
        if (type == PType::breakpoint) {
            breakTable[nbp.file + std::to_string(nbp.line)] = nbp.idx;
            llvm::outs() << "Breakpoint " << nbp.idx << " at " << nbp.file << ", line " << nbp.line << "\n";
        } else {
            killTable[nbp.file + std::to_string(nbp.line)] = nbp.idx;
            llvm::outs() << "Killpoint " << nbp.idx << " at " << nbp.file << ", line " << nbp.line << "\n";
        }
        Breakpoint::cnt++;
    } else {
        llvm::outs() << "Breakpoint(killpoint) already exists\n";
    }
}

void KDebugger::processDelete(std::string &) {
    unsigned sz = breakpointNumbers.size();
    if (sz) {
        breakpoints.erase(std::remove_if(breakpoints.begin(), breakpoints.end(), [&](const Breakpoint &bp){
            auto it = std::find(breakpointNumbers.begin(), breakpointNumbers.end(), bp.idx);
            if (it != breakpointNumbers.end()) {
                std::string fileLine = bp.file + std::to_string(bp.line);
                if (bp.type == PType::breakpoint) {
                    breakTable.erase(fileLine);
                } else {
                    killTable.erase(fileLine);
                }
                breakpointNumbers.erase(it);
                return true;
            }
            return false;
        }), breakpoints.end());
        llvm::outs() << (sz - breakpointNumbers.size()) << " breakpoint(s)/killpoints(s) successfully removed.\n";
        if (breakpointNumbers.size()) {
            llvm::outs() << "No breakpoint(s)/killpoint(s) with the following number(s):\n";
            for (auto num : breakpointNumbers) {
                llvm::outs() << num << " ";
            }
            llvm::outs() << "\n Type info break to list all breakpoints.\n";
        }
    } else {
        breakTable.clear();
        killTable.clear();
        breakpoints.clear();
        llvm::outs() << "All breakpoints removed.\n";
    }
}

void KDebugger::processPrint(std::string &) {
    llvm::outs() << "Printing variable: " << var << "\n";
    llvm::MDNode *md = searcher->currentState()->prevPC->inst->getMetadata("dbg");
    llvm::Value *scope = nullptr;
    if (md) {
        scope = md->getOperand(2);
    }
    auto &stack = searcher->currentState()->stack;
    auto &sf = stack.back();
    auto svalue = sf.st.lookup(var, scope);
    if (!svalue) {
        llvm::outs() << "Unable to print variable information\n";
        return;
    }
    if (svalue != nullptr && svalue->hasAddress) {
        auto state = searcher->currentState();
        Expr::Width type = executor->getWidthForLLVMType(svalue->type);
        auto addr = svalue->address;
        if (!isa<ConstantExpr>(addr))
            addr = state->constraints.simplifyExpr(addr);
        ObjectPair op;
        bool success;
        state->addressSpace.resolveOne(*state, executor->solver, addr, op, success);
        if (success) {
            auto mo = op.first;
            auto os = op.second;
            llvm::outs() << os->read(mo->getOffsetExpr(addr), type) << "\n";
            return;
        }
    } else {
        auto state = searcher->currentState();
        int regIdx = -1;
        llvm::Value *value = nullptr;
        if (svalue && svalue->hasValue) value = svalue->value;
        for (unsigned i = 0; i < sf.kf->numInstructions; ++i) {
            if (state->pc == sf.kf->instructions[i]) {
                break;
            }
            std::string temp = sf.kf->instructions[i]->inst->getName().str();
            auto res = std::mismatch(var.begin(), var.end(), temp.begin());
            if (sf.kf->instructions[i]->inst == value || res.first == var.end()) {
                regIdx = sf.kf->instructions[i]->dest;
            }
        }
        if (regIdx != -1) {
            llvm::outs() << sf.locals[regIdx].value << "\n";
        } else if (value) {
			llvm::outs() << "Use value\n";
            value->dump();
        } else {
            llvm::outs() << "Unable to print variable information\n";
        }
    }
}

void KDebugger::processPrintRegister(std::string&) {
    auto state = searcher->currentState();
    auto &sf = state->stack.back();
    auto kf = sf.kf;
    if (regNumber < 1 || regNumber > (int)kf->numRegisters) {
        llvm::outs() << "Only registers 1 - " << kf->numRegisters << " are available\n";
        return;
    }
    llvm::outs() << "Printing contents of register #" << regNumber << "\n";
    auto expr = sf.locals[regNumber - 1].value;
    if (expr.isNull()) {
        llvm::outs() << "Null\n";
    } else {
        llvm::outs() << expr << "\n";
    }
}

void KDebugger::processCodeListing(std::string &) {
    int lineNumber = 0;
    std::string file = {};
    bool lineProvided = false;
    auto state = searcher->currentState();
    if (location == "") {
        lineNumber = state->pc->info->line;
        file = state->pc->info->file;
    } else {
        std::stringstream ss(location);
        if (ss >> lineNumber) {
            lineProvided = true;
        }
    }

    if (file == "") {
        auto inst= state->pc->inst;
        auto f = inst->getParent()->getParent();

        llvm::DebugInfoFinder DbgFinder;
        DbgFinder.processModule(*f->getParent());
        for (llvm::DebugInfoFinder::iterator fIt = DbgFinder.subprogram_begin(),
                                        fItE = DbgFinder.subprogram_end(); fIt != fItE; ++fIt) {
            llvm::DISubprogram SubP(*fIt);
            if (SubP.getFunction() == 0)
            continue;
            if (!lineProvided && SubP.getFunction()->getName().str() == location) {
                file = SubP.getFilename();
                lineNumber = SubP.getLineNumber();
                llvm::outs() << lineNumber << "\n";
                break;
            }
            if (lineProvided && SubP.getFunction() == f) {
                file = SubP.getFilename();
                break;
            }
        }
    }

    int from = std::max(1, lineNumber - 4);
    int to = lineNumber + (9 - (lineNumber - from));

    std::string code = getSourceLines(file, from, to);
    if (code != "") {
        llvm::outs() << code;
        location = {};
        return;
    }
    if (lineProvided) {
        llvm::outs() << "Line " << lineNumber << " ";
    } else {
        llvm::outs() << "Function " << location << " ";
    }
    llvm::outs() << "out of range.\n";
    location = {};
}

void KDebugger::processSet(std::string &) {
    llvm::MDNode *md = searcher->currentState()->prevPC->inst->getMetadata("dbg");
    llvm::Value *scope = nullptr;
    if (md) {
        scope = md->getOperand(2);
    }
    auto &stack = searcher->currentState()->stack;
    auto &sf = stack.back();
    auto svalue = sf.st.lookup(var, scope);
    if (!svalue) {
        llvm::outs() << "Unable to find variable " << var << "\n";
        return;
    }
    if (svalue != nullptr && svalue->hasAddress) {
        auto state = searcher->currentState();
        // Expr::Width type = executor->getWidthForLLVMType(svalue->type);
        auto addr = svalue->address;
        if (!isa<ConstantExpr>(addr))
            addr = state->constraints.simplifyExpr(addr);
        ObjectPair op;
        bool success;
        state->addressSpace.resolveOne(*state, executor->solver, addr, op, success);
        if (success) {
            executor->executeMakeSymbolic(*searcher->currentState(), op.first, var);
            llvm::outs() << var << " is now symbolic\n";
            return;
        }
    }

    llvm::outs() << "Unable to find variable " << var << "\n";
}

void KDebugger::processInfo(std::string &) {
    auto state = searcher->currentState();
    switch (infoOpt) {
        case InfoOpt::stack: printStack(state); break;
        case InfoOpt::constraints: printConstraints(state); break;
        case InfoOpt::killpoints:
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
        if (stateAddrHex.size() >= 2 && stateAddrHex[0] == '0' && stateAddrHex[1] == 'x') {
            stateAddrHex = stateAddrHex.substr(2);
        }
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
    const unsigned locationCol = 9;
    llvm::formatted_raw_ostream fo(llvm::outs());
    fo << "Num";
    fo.PadToColumn(locationCol);
    fo << "Location\n";
    for (auto & bp : breakpoints) {
        if (bp.type == PType::breakpoint && infoOpt == InfoOpt::killpoints) {
            continue;
        }
        if (bp.type == PType::killpoint && infoOpt == InfoOpt::breakpoints) {
            continue;
        }
        fo << bp.idx;
        fo.PadToColumn(locationCol);
        fo << bp.file << ":" << bp.line << "\n";
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

void KDebugger::printCode(ExecutionState *state, bool printAssembly) {
    assert(state);

    auto file = state->pc->info->file;
    auto line = state->pc->info->line;

    if (file == "") {
        llvm::outs()  << "No corresponding source information\n";
    } else {
        std::string source = getSourceLine(file, line);
		if (source == "") {
			llvm::outs()  << "No corresponding source information\n";
		} else {
			size_t first = source.find_first_not_of(' ');
			std::string trimmed = source.substr(first, source.size() - first + 1);
			llvm::outs() << "loc: " << file << ":" << line << "\n";
			llvm::outs() << "source: " << trimmed << "\n";
		}
    }
    if (printAssembly) {
        llvm::outs() << "assembly:";
        state->pc->inst->print(llvm::outs());
    }
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
