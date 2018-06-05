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
#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/ExecutionCommand.h"
#include "klee/Debugger/BreakpointCommand.h"
#include "klee/Debugger/HelpCommand.h"
#include "klee/Debugger/InfoCommand.h"
#include "klee/Debugger/StateCommand.h"
#include "klee/Debugger/PrintCommand.h"
#include "klee/Debugger/SetCommand.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Debugger/DebugSymbolTable.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Statistics.h"

namespace klee {

namespace {

const char *MSG_INTERRUPTED
    = "(kleedb, interrupted) ";
const char *MSG_SELECT_STATE
    = "(Select a state to continue) ";
const int itemsPerPage = 4;
}

extern "C" void set_interrupted(bool);
extern "C" bool klee_interrupted();

KDebugger::KDebugger() :
    prompt(this),
    commands(0),
    statsTracker(0),
    breakpoints(),
    step(true),
    stepi(true),
    stopUponBranching(false),
    stopOnError(false),
    printStateOpt(PrintStateOption::DEFAULT) {}

KDebugger::~KDebugger() {
    commands->destroy();
    delete commands;
}

void KDebugger::init() {
    commands = new DebugCommandList();
    commands->addSubCommand(new ContinueCommand(this));
    commands->addSubCommand(new QuitCommand(executor));
    commands->addSubCommand(new StepCommand(this, this->dbgSearcher));
    commands->addSubCommand(new StepInstructionCommand(this, this->dbgSearcher));
    commands->addSubCommand(new BreakpointCommand(&breakpoints, executor->kmodule->infos, executor->kmodule));
    commands->addSubCommand(new KillpointCommand(&breakpoints, executor->kmodule->infos, executor->kmodule));
    commands->addSubCommand(new DeleteCommand(&breakpoints));
    InfoCommand *infoCmd = new InfoCommand();
    infoCmd->addSubCommand(new InfoBreakpointCommand(&breakpoints));
    infoCmd->addSubCommand(new InfoStatsCommand(statsTracker));
    infoCmd->addSubCommand(new InfoStackCommand(dbgSearcher));
    infoCmd->addSubCommand(new InfoConstraintsCommand(dbgSearcher));
    infoCmd->addSubCommand(new InfoStateCommand(dbgSearcher));
    infoCmd->addSubCommand(new InfoStatesCommand(dbgSearcher));
    StateCommand *stateCmd = new StateCommand();
    stateCmd->addSubCommand(new StateCycleCommand(dbgSearcher, this));
    stateCmd->addSubCommand(new StateMoveCommand(dbgSearcher, this));
    commands->addSubCommand(infoCmd);
    commands->addSubCommand(stateCmd);
    commands->addSubCommand(new TerminateCommand(executor, dbgSearcher, this));
    commands->addSubCommand(new GenerateInputCommand(executor, dbgSearcher));
    commands->addSubCommand(new PrintCommand(executor, dbgSearcher));
    commands->addSubCommand(new ListCodeCommand(dbgSearcher, executor->kmodule->infos));
    commands->addSubCommand(new SetSymbolicCommand(executor, dbgSearcher));
    commands->addSubCommand(new ToggleCompactCommand(&printStateOpt));
    commands->addSubCommand(new ToggleStopOnErrorCommand(&stopOnError));
    commands->addSubCommand(new HelpCommand(commands));
    prompt.setCommandList(commands);
}

void KDebugger::preprocess() {
    dbgSearcher->updateCurrentState();
    if (klee_interrupted()) {
        if (showPrompt() == -1) {
            return;
        }
    }

    if (dbgSearcher->stateBranched()) {
        if (alertBranching(step || stepi || stopUponBranching) == -1) {
            executor->setHaltExecution(true);
            return;
        }
    }
    ExecutionState *state = nullptr;
    state = dbgSearcher->currentState();
    auto pc = state->pc;
    auto last = state->lastStepped;

    int idx = checkBreakpoint(*state);
    if (idx) {
        // Make sure this is a breakpoint, then show prompt
        if (idx > 0) {
            state->lastStepped = pc;
            showPrompt();
        }
        return;
    }

    if (!stepi && !step) return;
    if (step && pc->info->file == "") return;

    // First instruction of the function
    if (step && state->stack.back().kf->instructions[0] != pc && !pc->inst->getMetadata("dbg")) return;

    if (stepi || !last || pc->info->file != last->info->file || pc->info->line != last->info->line) {
        state->lastStepped = pc;
        showPrompt();
    }
}

void KDebugger::onError() {
    if (stopOnError) {
        auto state = dbgSearcher->currentState();
        state->lastStepped = state->pc;
        showPrompt();
    }
}

int KDebugger::alertBranching(bool askForSelection) {
    auto &addedStates = dbgSearcher->getaddedStates();
    llvm::outs().changeColor(llvm::raw_ostream::GREEN);
    llvm::outs() << "Execution branched from:\n";
    debugutil::printCode(dbgSearcher->currentState()->prevPC, PrintCodeOption::SOURCE_ONLY);
    llvm::outs() << "\n";
    unsigned int cnt = 0;
    for (auto state : addedStates) {
        if (askForSelection) {
            if (printStateOpt == PrintStateOption::DEFAULT) {
                for (int i = 0; i < 15; ++i) llvm::outs() << "-";
                llvm::outs() << "  ";
            }
            llvm::outs() << "Enter " << ++cnt << " to select the following state ";
            for (int i = 0; i < 15; ++i) llvm::outs() << "-"; llvm::outs() << "\n";
        } else {
            if (printStateOpt == PrintStateOption::DEFAULT) {
                for (int i = 0; i < 25; ++i) llvm::outs() << "-";
                llvm::outs() << "  ";
            }
            llvm::outs() << "Branch " << ++cnt << "  ";
            for (int i = 0; i < 25; ++i) llvm::outs() << "-"; llvm::outs() << "\n";
        }
        debugutil::printState(state, printStateOpt);
        llvm::outs() << "\n";
    }
    if (askForSelection) {
        int res = prompt.askForSelection(MSG_SELECT_STATE, 1, addedStates.size());
        if (res == -1) {
            return -1;
        }
        selectBranch(res);
        step = stepi = true;
    } else {
        llvm::outs().changeColor(llvm::raw_ostream::CYAN);
        dbgSearcher->unlockState();
        auto state = &dbgSearcher->selectState();
        dbgSearcher->setCurrentState(state);
        llvm::outs() << "Continuing execution from state @" << dbgSearcher->currentState() << "\n";
        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        executor->setHaltExecution(false);
    }
    return 0;
}

int KDebugger::showPrompt() {
    int res = 0;
    step = stepi = false;
    if (klee_interrupted()) {
        set_interrupted(false);
        executor->setHaltExecution(false);
        res = prompt.show(MSG_INTERRUPTED);
    } else {
        res = prompt.show();
    }
    if (res == -1) {
        executor->setHaltExecution(true);
    }
    if (executor->haltExecution) {
        return -1;
    }
    return res;
}

int KDebugger::checkBreakpoint(ExecutionState &state) {
    int idx = state.pc->breakPoint;
    auto info = state.pc->info;
    if (idx < 0) {
        llvm::outs().changeColor(llvm::raw_ostream::RED);
        llvm::outs() << "\nKillpoint " << (-idx) << ", at "  << info->file << ":" << info->line << "\n";
        llvm::outs() << "Terminating current state @" << &state << "\n";
        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        std::vector<std::string> input = {"terminate"};
        ParsingResult pRes;
        CommandResult res;
        commands->parse(input, pRes);
        pRes.command->execute(res);
        return idx;
    }
    if (idx > 0) {
        llvm::outs() << "\nBreakpoint " << idx << ", at "  << info->file << ":" << info->line << "\n";
        debugutil::printCode(state.pc, PrintCodeOption::SOURCE_ONLY);
        return idx;

    }
    return 0;
}

void KDebugger::selectBranch(int idx) {
    dbgSearcher->lockState();
    dbgSearcher->setCurrentState(
        dbgSearcher->getaddedStates().at(idx - 1),
        /* lock state = */ true
    );
    auto state = dbgSearcher->currentState();
    llvm::outs() << "You selected state @" << state << ".\n";
    stopUponBranching = false;
    prompt.breakFromLoop();
}

}
