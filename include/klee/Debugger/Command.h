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
    del,
    print,
    set,
    info, 
    state, 
    terminate,
    generate_input,
    help, 
    none
};

enum class InfoOpt {
    stack,
    constraints,
    state,
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

enum class TerminateOpt {
    current,
    other,
    invalid
};

enum class GenerateInputOpt {
    char_array,
    integer,
    invalid
};

namespace debugcommands {

extern CommandType selected;
extern std::vector<std::string> extraArgs;
extern bool stopUponBranching;
extern int stateIdx;
extern unsigned breakpointIdx;
extern std::string bpString;
extern std::string var;
extern std::string stateAddrHex;
extern InfoOpt infoOpt;
extern StateOpt dir;
extern TerminateOpt termOpt;
extern GenerateInputOpt generateInputOpt;

extern clipp::group continueCmd;
extern clipp::group runCmd;
extern clipp::group stepCmd;
extern clipp::group quitCmd;
extern clipp::group helpCmd;
extern clipp::group breakCmd;
extern clipp::group infoCmd;
extern clipp::group terminateCmd;
extern clipp::group stateCmd;
extern clipp::group cmds[13];
extern clipp::group cmdParser;
extern clipp::group branchSelection;

}

}
#endif