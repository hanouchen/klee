#ifndef KLEE_INFOCOMMAND_H
#define KLEE_INFOCOMMAND_H

#include <string>
#include "klee/Debugger/DebugCommand.h"
#include "klee/Debugger/Breakpoint.h"

namespace klee {

class DebugSearcher;
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
    InfoStackCommand(DebugSearcher *searcher);
    virtual std::string getName() { return "info stack"; }
    virtual void execute(CommandResult &res);
private:
    DebugSearcher *searcher;
};

class InfoConstraintsCommand : public DebugCommandObject {
public:
    InfoConstraintsCommand(DebugSearcher *searcher);
    virtual std::string getName() { return "info constraints"; }
    virtual void execute(CommandResult &res);
private:
    DebugSearcher *searcher;
};

class InfoStateCommand : public DebugCommandObject {
public:
    InfoStateCommand(DebugSearcher *searcher);
    virtual std::string getName() { return "info state"; }
    virtual void execute(CommandResult &res);
private:
    DebugSearcher *searcher;
};

class InfoStatesCommand : public DebugCommandObject {
public:
    InfoStatesCommand(DebugSearcher *searcher);
    virtual std::string getName() { return "info states"; }
    virtual void execute(CommandResult &res);
private:
    virtual void displayLongList();
    DebugSearcher *searcher;
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