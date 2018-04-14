#include <assert.h>

#include <cstddef>
#include "klee/Debugger/linenoise.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/KleeDebugger.h"

namespace klee {

namespace {
void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'g') {
        linenoiseAddCompletion(lc,"generate-input");
    } else if (buf[0] == 't') {
        linenoiseAddCompletion(lc,"terminate");
    }
}
}

extern "C" void set_halt_execution(bool);

Prompt::Prompt(KDebugger *debugger) : debugger(debugger), breakLoop(false) {
    linenoiseSetCompletionCallback(completion);
};

int Prompt::show(const char *line) {
    std::string msg(line);
    breakLoop = false;
    char *cmd;
    while((cmd = linenoise(msg.c_str())) != NULL) {
        if (cmd[0] != '\0') {
            linenoiseHistoryAdd(cmd);
            std::istringstream iss(cmd);
            std::vector<std::string> tokens;
            std::copy(std::istream_iterator<std::string>(iss),
                    std::istream_iterator<std::string>(),
                    back_inserter(tokens));
            
            debugger->handleCommand(tokens, msg);
        }
        linenoiseFree(cmd); 
        if (breakLoop) {
            break;
        }
    }
    if (!breakLoop) {
        set_halt_execution(true);
    }
    return breakLoop ? 0 : -1;
}

void Prompt::breakFromLoop() {
    breakLoop = true;
}
};