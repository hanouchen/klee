#ifndef KLEE_COMMAND_H
#define KLEE_COMMAND_H
#include <iostream>
#include <assert.h>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <sstream>
#include <algorithm>
#include <iterator>

namespace klee {

enum class CommandType {
    cont, 
    run, 
    step,
    quit, 
    breakpoint, 
    info, 
    help, 
    state, 
    none
};

enum class InfoOpt {
    stack,
    constraints,
    all,
    breakpoints,
    statistics,
    invalid
};

enum class StateOpt {
    next,
    prev,
    invalid
};

}
#endif