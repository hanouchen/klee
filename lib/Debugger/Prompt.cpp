#include <assert.h>

#include <cstddef>
#include "klee/Debugger/linenoise.h"
#include "klee/Debugger/Prompt.h"

namespace klee {

Prompt::Prompt(CommandHandler handler) : handler(handler), breakLoop(false) {};

void Prompt::show(const char *line) {
    char *cmd;
    while((cmd = linenoise(line)) != NULL) {
        CommandBuffer cmdBuff(cmd);
        handler(cmdBuff);
        if (cmd[0] != '\0') {
            linenoiseHistoryAdd(cmd);
        }
        linenoiseFree(cmd); 
        if (breakLoop) {
            breakLoop = false;
            break;
        }
    }
}

void Prompt::breakFromLoop() {
    breakLoop = true;
}
};