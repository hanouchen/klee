#ifndef KLEE_HELPCOMMAND_H
#define KLEE_HELPCOMMAND_H

#include <string>
#include <vector>
#include "klee/Debugger/DebugCommand.h"

namespace klee {

class HelpCommand : public DebugCommandObject {
public:
    HelpCommand(DebugCommand *commands);
    virtual std::string getName() { return "help"; }
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res);
    virtual void execute(CommandResult &res);

private:
    std::vector<std::string> input;
    DebugCommand *commands;
};

};
#endif