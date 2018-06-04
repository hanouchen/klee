#ifndef KLEE_BREAKPOINTCOMMAND_H
#define KLEE_BREAKPOINTCOMMAND_H

#include <string>
#include <unordered_map>

#include "klee/Debugger/DebugCommand.h"
#include "klee/Debugger/Breakpoint.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KModule.h"

namespace klee {

class StopCommand : public DebugCommandObject {
public:
    StopCommand(std::vector<Breakpoint> *bps,
                BreakpointType type,
                InstructionInfoTable *infos,
                KModule *module);
    virtual void execute(CommandResult &res);

protected:
    std::string location;
    std::vector<Breakpoint> *breakpoints;
    BreakpointType type;
    InstructionInfoTable *infos;
    KModule *module;
};

class BreakpointCommand : public StopCommand {
public:
    BreakpointCommand(std::vector<Breakpoint> *bps,
                      InstructionInfoTable *infos,
                      KModule *module);
    virtual std::string getName() { return "breakpoint"; }
};

class KillpointCommand : public StopCommand {
public:
    KillpointCommand(std::vector<Breakpoint> *bps,
                     InstructionInfoTable *infos,
                     KModule *module);
    virtual std::string getName() { return "breakpoint"; }
};

class DeleteCommand : public DebugCommandObject {
public:
    DeleteCommand(std::vector<Breakpoint> *bps);
    virtual std::string getName() { return "delete"; }
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res);
    virtual void execute(CommandResult &res);

private:
    std::vector<unsigned> numbers;
    std::string location;
    std::vector<Breakpoint> *breakpoints;
};


};

#endif