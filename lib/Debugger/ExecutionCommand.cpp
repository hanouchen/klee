#include "klee/Debugger/ExecutionCommand.h"

#include <string>

#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/KleeDebugger.h"
#include "../lib/Core/Executor.h"
#include "../lib/Core/Searcher.h"
#include "clang/AST/Expr.h"

namespace klee {
ContinueCommand::ContinueCommand(KDebugger *debugger) :
    debugger(debugger),
    stopOnBranching(false) {

    command = clipp::group(clipp::command("continue", "c"));

    parser = (
        clipp::group(command),
        clipp::option("-s").set(stopOnBranching, true).doc("stop when execution branches"),
        clipp::any_other(extraArgs)
    );

    command.doc("Continue execution of KLEE");
    command.push_back(clipp::any_other(extraArgs));
}

void ContinueCommand::execute(CommandResult &res) {
    debugger->setStopUponBranching(stopOnBranching);
    res.success = true;
    res.stayInDebugger = false;
    stopOnBranching = false;
    res.setMsg("Continuing execution\n");
}


QuitCommand::QuitCommand(Executor *executor) :
    executor(executor) {

    command = clipp::group(clipp::command("quit", "q"));

    parser = (
        clipp::group(command),
        clipp::any_other(extraArgs)
    );

    command.doc("Terminate klee and exit the debugger");
    command.push_back(clipp::any_other(extraArgs));
}

void QuitCommand::execute(CommandResult &res) {
    executor->setHaltExecution(true);
    res.success = true;
    res.stayInDebugger = false;
    res.setMsg("Quitting klee and the debugger\n");
}

StepCommand::StepCommand(KDebugger *debugger, DbgSearcher *searcher) :
    debugger(debugger),
    searcher(searcher) {

    command = clipp::group(clipp::command("step", "s"));

    parser = (
        clipp::group(command),
        clipp::any_other(extraArgs)
    );
    command.doc("Execute one line of source code");
    command.push_back(clipp::any_other(extraArgs));
}

void StepCommand::execute(CommandResult &res) {
    debugger->setStepping();
    auto state = searcher->currentState();
    debugutil::printCode(state->pc, PrintCodeOption::LOC_SOURCE);
    res.success = true;
    res.stayInDebugger = false;
}


StepInstructionCommand::StepInstructionCommand(KDebugger *debugger, DbgSearcher *searcher) :
    debugger(debugger),
    searcher(searcher) {

    command = clipp::group(clipp::command("stepi", "si"));

    parser = (
        clipp::group(command),
        clipp::any_other(extraArgs)
    );
    command.doc("Execute one llvm instruction");
    command.push_back(clipp::any_other(extraArgs));
}

void StepInstructionCommand::execute(CommandResult &res) {
    debugger->setSteppingInstruction();
    auto state = searcher->currentState();
    debugutil::printCode(state->pc);
    res.success = true;
    res.stayInDebugger = false;
}

};