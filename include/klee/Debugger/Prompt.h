#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <functional>
#include "klee/Debugger/Command.h"

namespace klee {

namespace {
const char *DEFAULT = "klee debugger, type h for help> ";
};

typedef std::function<void(CommandBuffer &)> CommandHandler;

class Prompt {

public:
    Prompt(CommandHandler handler);
    void show(const char *line = DEFAULT);
    void breakFromLoop();

private:    
    CommandHandler handler;
    bool breakLoop;
};

};

#endif
