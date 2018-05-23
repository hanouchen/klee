#ifndef KLEE_EXECUTIONCOMMAND_H
#define KLEE_EXECUTIONCOMMAND_H

#include <string>
#include "klee/Debugger/DebugCommand.h"

#include "klee/Debugger/clipp.h"
#include "../../../lib/Core/Executor.h"
#include "../../../lib/Core/Searcher.h"
#include "klee/Debugger/KleeDebugger.h"

namespace klee {

class ContinueCommand : public DebugCommandObject {

public:
    ContinueCommand(KDebugger *debugger);
    std::string getName() { return "continue"; }
    virtual void execute(CommandResult &res);

private:
    KDebugger *debugger;
    bool stopOnBranching;
};

class QuitCommand : public DebugCommandObject {

public:
    QuitCommand(Executor *executor);
    std::string getName() { return "quit"; }
    virtual void execute(CommandResult &res);

private:
    Executor *executor;
};

class StepCommand : public DebugCommandObject {

public:
    StepCommand(KDebugger *debugger, DebugSearcher *searcher);
    std::string getName() { return "step"; }
    virtual void execute(CommandResult &res);

private:
    KDebugger *debugger;
    DebugSearcher *searcher;
};


class StepInstructionCommand : public DebugCommandObject {

public:
    StepInstructionCommand(KDebugger *debugger, DebugSearcher *searcher);
    std::string getName() { return "step"; }
    virtual void execute(CommandResult &res);

private:
    KDebugger *debugger;
    DebugSearcher *searcher;
};

};
#endif