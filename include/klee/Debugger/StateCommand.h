#ifndef KLEE_STATECOMMAND_H
#define KLEE_STATECOMMAND_H

#include <string>
#include "klee/Debugger/Breakpoint.h"
#include "klee/Debugger/DebugCommand.h"
#include "../../../lib/Core/Executor.h"
#include "../../../lib/Core/Searcher.h"

namespace klee {

class StateCommand : public DebugCommandList {
public:
    StateCommand();
    virtual std::string getName() { return "state"; }
    virtual void execute(CommandResult &res) {}
};

class StateCycleCommand : public DebugCommandObject {
public:
    StateCycleCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "state cycle"; }
    virtual void execute(CommandResult &res);

private:
    bool forward;
    DbgSearcher *searcher;
};

class StateMoveCommand : public DebugCommandObject {
public:
    StateMoveCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "state move"; }
    virtual void execute(CommandResult &res);

private:
    std::string addressStr;
    DbgSearcher *searcher;
};

class ToggleCompactCommand : public DebugCommandObject {
public:
    ToggleCompactCommand(PrintStateOption *opt);
    virtual std::string getName() { return "toggle compact"; }
    virtual void execute(CommandResult &res);
private:
    PrintStateOption *opt;
};

class TerminateCommand : public DebugCommandObject {
public:
    TerminateCommand(Executor *executor, DbgSearcher *searcher);
    virtual std::string getName() { return "terminate"; }
    virtual void execute(CommandResult &res);

private:
    bool terminateCurrent;
    Executor *executor;
    DbgSearcher *searcher;
};

class GenerateInputCommand : public DebugCommandObject {
public:
    GenerateInputCommand(Executor *executor, DbgSearcher *searcher);
    virtual std::string getName() { return "generate-input"; }
    virtual void execute(CommandResult &res);

private:
    Executor *executor;
    DbgSearcher *searcher;
};


};

#endif