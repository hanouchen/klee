#include <assert.h>

#include <iostream>
#include <sstream>
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

Prompt::Prompt(KDebugger *debugger) :
        debugger(debugger),
        breakLoop(false),
        lastNonEmptyCmd() {
    linenoiseSetCompletionCallback(completion);
};

int Prompt::show(const char *line) {
    std::string msg(line);
    breakLoop = false;
    char *cmd;
    while((cmd = linenoise(msg.c_str())) != NULL) {
        if (cmd[0] == '\0' && lastNonEmptyCmd.empty()) {
            continue;
        }
        std::vector<std::string> tokens;
        if (cmd[0] != '\0') {
            linenoiseHistoryAdd(cmd);
            std::istringstream iss(cmd);
            std::copy(std::istream_iterator<std::string>(iss),
                    std::istream_iterator<std::string>(),
                    back_inserter(tokens));
            lastNonEmptyCmd = tokens;
        } else {
            tokens = lastNonEmptyCmd;
        }
        ParsingResult parseRes;
        commands->parse(tokens, parseRes);
        if (parseRes.success) {
            CommandResult res;
            parseRes.command->execute(res);
            llvm::outs() << res.msg;
            breakLoop = !res.stayInDebugger;
        } else {
            llvm::outs() << parseRes.errMsg;
            std::string prefix = {};
            if (!parseRes.command) {
                parseRes.command = commands;
                prefix = "COMMANDS:\n";
            }
            llvm::outs() << prefix << parseRes.command->usageString();
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

int Prompt::askForSelection(const char *msg, int from, int to) {
    int num = 0;
    bool success = false;
    char *in;
    while((in = linenoise(msg)) != NULL) {
        std::string str(in);
        std::stringstream ss(str);
        ss >> num;
        if (!ss || num < from || num > to) {
            llvm::outs() << "Please enter a number between [" << from << ", " << to << "]\n";
        } else {
            success = true;
            break;
        }
        linenoiseFree(in);
    }

    linenoiseFree(in);
    num = success ? num : -1;
    return num;
}

void Prompt::setCommandList(DebugCommandList *commands) {
    this->commands = commands;
}

void Prompt::breakFromLoop() {
    breakLoop = true;
}
};