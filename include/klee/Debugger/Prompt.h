#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <vector>
#include <string>
#include "klee/Debugger/Command.h"

namespace klee {
class KDebugger;

class Prompt {

public:
    Prompt(KDebugger *);
    int show(const char *line);
    void breakFromLoop();

private:
    KDebugger *debugger;
    bool breakLoop;
};

};

#endif
