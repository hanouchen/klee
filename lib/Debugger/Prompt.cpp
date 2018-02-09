#include <assert.h>

#include <cstddef>
#include "klee/Debugger/linenoise.h"
#include "klee/Debugger/Prompt.h"

namespace klee {

Prompt::Prompt(CommandHandler handler) : handler(handler), breakLoop(false) {};

int Prompt::show(const char *line) {
    breakLoop = false;
    char *cmd;
    while((cmd = linenoise(line)) != NULL) {
        if (cmd[0] != '\0') {
            linenoiseHistoryAdd(cmd);
            std::istringstream iss(cmd);
            std::vector<std::string> tokens;
            std::copy(std::istream_iterator<std::string>(iss),
                    std::istream_iterator<std::string>(),
                    back_inserter(tokens));
            handler(tokens);
        }
        linenoiseFree(cmd); 
        if (breakLoop) {
            break;
        }
    }
    return breakLoop ? 0 : -1;
}

void Prompt::breakFromLoop() {
    breakLoop = true;
}
};