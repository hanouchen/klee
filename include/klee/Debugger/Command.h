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
    killpoint,
    del,
    print,
    print_register,
    list,
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
    killpoints,
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
extern std::vector<unsigned> breakpointNumbers;
extern std::string bpString;
extern std::string var;
extern std::string stateAddrHex;
extern InfoOpt infoOpt;
extern StateOpt dir;
extern TerminateOpt termOpt;
extern GenerateInputOpt generateInputOpt;
extern std::string location;
extern int regNumber;

extern clipp::group cmds[16];
extern clipp::group cmdParser;
extern clipp::group branchSelection;

}

}
#endif