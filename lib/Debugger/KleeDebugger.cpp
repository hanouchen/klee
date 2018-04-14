#include <assert.h>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <vector>

// FIXME: Probably not a good idea?
#include "../Core/Searcher.h"
#include "../Core/StatsTracker.h"
#include "../Core/Executor.h"
#include "../Core/Memory.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
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
    std::cmatch matches;
    std::regex fileRegex(".*\\/(\\w*\\.\\w*)");
    if (std::regex_search(fullPath.c_str(), matches, fileRegex)) {
        return matches[1].str();
    }
    return "Unkown file";
}

const char *MSG_INTERRUPTED 
    = "KLEE: ctrl-c detected, execution interrupted> ";
const char *MSG_SELECT_STATE
    = "Select a state to continue by entering state number:";

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
    &KDebugger::processPrint,
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
    step(true) {}

void KDebugger::selectState() {
    if (klee_interrupted()) {
        step = false;
        set_interrupted(false);
        set_halt_execution(false);
        prompt.show(MSG_INTERRUPTED);
    } else if (step) {
        // Check if execution branched.
        if (searcher->newStates()) {
            unsigned int newStates = searcher->newStates();
            auto states = searcher->getStates();
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "Execution branched\n";
            unsigned int cnt = 0;
            for (auto it = states.end() - newStates - 1; it != states.end(); ++it) {
                llvm::outs().changeColor(llvm::raw_ostream::CYAN);
                llvm::outs() << "Enter " << ++cnt << " to select ";
                printState(*it);
                llvm::outs() << "\n";
            }
            if (prompt.show(MSG_SELECT_STATE)) {
                return;
            }
        }
        step = false;
        showPromptAtInstruction(searcher->currentState()->pc);
    } else {
        checkBreakpoint(*searcher->currentState());
    }
}

void KDebugger::showPrompt(const char *msg) {
    assert(msg);
    prompt.show(msg);
}

void KDebugger::showPromptAtInstruction(const KInstruction *ki) {
    std::string file = getFileFromPath(ki->info->file);
    std::string msg = file + ", line " + std::to_string(ki->info->line) + "> ";
    prompt.show(msg.c_str());
}

void KDebugger::checkBreakpoint(ExecutionState &state) {
    if (breakpoints.empty() || !state.pc) {
        return;
    }
    auto ki = state.pc;
    Breakpoint bp(getFileFromPath(ki->info->file), ki->info->line);
    if (state.lastBreakpoint != bp && breakpoints.find(bp) != breakpoints.end()) {
        state.lastBreakpoint = bp;
        showPromptAtInstruction(ki);
    }
}

void KDebugger::setSearcher(DebugSearcher *searcher) {
    this->searcher = searcher;
}

void KDebugger::handleCommand(std::vector<std::string> &input, std::string &msg) {
    if (step && searcher->newStates()) {
        auto res = clipp::parse(input, branchSelection);
        if (res) {
            selectBranch(stateIdx, msg);
        }
        return;
    }
    auto res = clipp::parse(input, cmdParser);
    if (res && extraArgs.empty()) {
        (this->*(KDebugger::processors[(int)selected]))(msg);
    } else {
        if (extraArgs.empty() && res.any_blocked()) {
            llvm::outs() << "Unkown command: \"" << input[0] << "\"\n";
        }
        for (auto const & arg: extraArgs) {
            llvm::outs() << "Unsupported arguments: " << arg << "\n";
        }
        extraArgs.clear();
    }
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
    std::regex breakpointRegex("(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    if (std::regex_search(bpString.c_str(), matches, breakpointRegex)) {
        auto res = breakpoints.emplace(matches.str(1), (unsigned)stoi(matches.str(2)));
        klee_message(res.second ? "Breakpoint set" : "Breakpoint already exist");
    } else {
        klee_message("Invalid breakpoint format");
    }
}

void KDebugger::processPrint(std::string &) {
    llvm::outs() << "Printing variable: " << var << "\n";
    auto objs = searcher->currentState()->addressSpace.objects;
    auto st = searcher->currentState()->stack;
    auto stackFrame = searcher->currentState()->stack.back();
    auto func = stackFrame.kf->function;
    llvm::ValueSymbolTable &symbolTable = func->getValueSymbolTable();
    llvm::StringRef sRef(var.c_str());
    auto *val = symbolTable.lookup(sRef);
    val->dump();
}

void KDebugger::processInfo(std::string &) {
    auto state = searcher->currentState();
    switch (infoOpt) {
        case InfoOpt::stack: printStack(state); break;
        case InfoOpt::constraints: printConstraints(state); break;
        case InfoOpt::breakpoints: printBreakpoints(); break;
        case InfoOpt::states: printAllStates(); break;
        case InfoOpt::statistics: this->statsTracker->printStats(llvm::outs()); break;
        case InfoOpt::state: printState(state); break;
        default:
            llvm::outs() << "Invalid info option, type \"h\" for help.\n";
    }
    infoOpt = InfoOpt::state;
}

void KDebugger::processState(std::string &msg) {
    searcher->nextIter();
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
    auto state = *(searcher->getStates().end() - newStates + idx - 2);
    llvm::outs() << "You selected state @" << state << ".\n";
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
    klee_message("%zu breakpoints set", breakpoints.size());
    for (auto & bp : breakpoints) {
        klee_message("%s at line %u", bp.file.c_str(), bp.line);
    }
}

void KDebugger::printAllStates() {
    llvm::outs().changeColor(llvm::raw_ostream::GREEN);
    llvm::outs() << "Total number of states: " << searcher->getStates().size();
    llvm::outs() << "\n";
    for (auto state : searcher->getStates()) {
        if (state == searcher->currentState()) {
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "current state --> ";
            llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        }
        printState(state);
        llvm::outs() << "\n";
    }
}

void KDebugger::printState(ExecutionState *state) {
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "state @" << state << ", ";
    printConstraints(state);
    llvm::outs() << "\n";
    printCode(state);
}

void KDebugger::printCode(ExecutionState *state) {
    auto info = state->pc->info;
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
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(llvm::outs());
}

void KDebugger::printConstraints(ExecutionState *state) {
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "constraints for state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), state->constraints);
}

}