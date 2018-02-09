#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <vector>
#include <string>
#include "klee/Debugger/Command.h"

namespace klee {

namespace {
const char *DEFAULT = "klee debugger, type h for help> ";
};

typedef std::function<void(std::vector<std::string> &)> CommandHandler;

class Prompt {

public:
    Prompt(CommandHandler handler);
    int show(const char *line = DEFAULT);
    void breakFromLoop();

private:    
    CommandHandler handler;
    bool breakLoop;
};

};

#endif
