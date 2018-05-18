#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <vector>
#include <string>
#include "klee/Debugger/Command.h"

namespace {
    const char* DEFAULT_MSG = "(kleedb) ";
}
namespace klee {
class KDebugger;

class Prompt {

public:
    Prompt(KDebugger *);
    int show(const char *line = DEFAULT_MSG);
    void breakFromLoop();

private:
    KDebugger *debugger;
    bool breakLoop;
    std::vector<std::string> lastNonEmptyCmd;
};

};

#endif
