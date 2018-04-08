#ifndef KLEE_COMMAND_H
#define KLEE_COMMAND_H
#include <klee/Debugger/clipp.h>

namespace klee {

enum CommandType {
    cont, 
    run, 
    step,
    quit, 
    breakpoint, 
    print,
    info, 
    state, 
    help, 
    none
};

enum class InfoOpt {
    stack,
    constraints,
    all,
    breakpoints,
    states,
    statistics,
    invalid
};

enum class StateOpt {
    next,
    prev,
    invalid
};

namespace debugcommands {

extern CommandType selected;
extern std::vector<std::string> extraArgs;
extern int stateIdx;
extern std::string bpString;
extern std::string var;
extern bool terminateOtherStates;
extern InfoOpt infoOpt;
extern StateOpt dir;

extern clipp::group continueCmd;
extern clipp::group runCmd;
extern clipp::group stepCmd;
extern clipp::group quitCmd;
extern clipp::group helpCmd;
extern clipp::group breakCmd;
extern clipp::group infoCmd;
extern clipp::group stateCmd;
extern clipp::group cmdParser;
extern clipp::group branchSelection;

}

}
#endif