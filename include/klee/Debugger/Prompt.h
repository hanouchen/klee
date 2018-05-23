#ifndef KLEE_PROMPT_H
#define KLEE_PROMPT_H

#include <vector>
#include <string>
#include "klee/Debugger/DebugCommand.h"

namespace {
    const char* DEFAULT_MSG = "(kleedb) ";
}
namespace klee {
class KDebugger;

class Prompt {

public:
    ~Prompt() {}
    Prompt(KDebugger *);
    void setCommandList(DebugCommandList *commands);
    int show(const char *line = DEFAULT_MSG);
    int askForSelection(const char *msg, int from, int to);
    void breakFromLoop();
    void addCommand(DebugCommand *command);

private:
    KDebugger *debugger;
    bool breakLoop;
    std::vector<std::string> lastNonEmptyCmd;
    DebugCommandList *commands;
};

};

#endif
