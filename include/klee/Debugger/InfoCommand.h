#ifndef KLEE_INFOCOMMAND_H
#define KLEE_INFOCOMMAND_H

#include <string>
#include "klee/Debugger/DebugCommand.h"
#include "klee/Debugger/Breakpoint.h"

namespace klee {

class DbgSearcher;
class StatsTracker;

class InfoCommand : public DebugCommandList {
public:
    InfoCommand();
    virtual std::string getName() { return "info"; }
    virtual void execute(CommandResult &res) {}
};

class InfoBreakpointCommand : public DebugCommandObject {
public:
    InfoBreakpointCommand(std::vector<Breakpoint> *bps);
    virtual std::string getName() { return "info breakpoint"; }
    virtual void execute(CommandResult &res);
private:
    std::vector<Breakpoint> *breakpoints;
    BreakpointType type;
};

class InfoStackCommand : public DebugCommandObject {
public:
    InfoStackCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "info stack"; }
    virtual void execute(CommandResult &res);
private:
    DbgSearcher *searcher;
};

class InfoConstraintsCommand : public DebugCommandObject {
public:
    InfoConstraintsCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "info constraints"; }
    virtual void execute(CommandResult &res);
private:
    DbgSearcher *searcher;
};

class InfoStateCommand : public DebugCommandObject {
public:
    InfoStateCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "info state"; }
    virtual void execute(CommandResult &res);
private:
    DbgSearcher *searcher;
};

class InfoStatesCommand : public DebugCommandObject {
public:
    InfoStatesCommand(DbgSearcher *searcher);
    virtual std::string getName() { return "info states"; }
    virtual void execute(CommandResult &res);
private:
    virtual void displayLongList(std::vector<ExecutionState *> &states);
    DbgSearcher *searcher;
    PrintStateOption opt;
};

class InfoStatsCommand : public DebugCommandObject {
public:
    InfoStatsCommand(StatsTracker *statsTracker);
    virtual std::string getName() { return "info state"; }
    virtual void execute(CommandResult &res);
private:
    StatsTracker *statsTracker;
};



};

#endif