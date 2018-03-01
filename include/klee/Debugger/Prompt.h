#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <vector>
#include <string>
#include "klee/Debugger/Command.h"

namespace klee {
class KDebugger;

namespace {
const char *DEFAULT = "klee debugger, type h for help> ";
};


class Prompt {

public:
    // static void parseInput(std::vector<std::string> &, const char *);
    Prompt(KDebugger *);
    int show(const char *line = DEFAULT);
    void breakFromLoop();

private:    
    KDebugger *debugger;
    bool breakLoop;
};

};

#endif
