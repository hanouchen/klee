#include "klee/Debugger/clipp.h"
#include "klee/Debugger/HelpCommand.h"

namespace klee {

HelpCommand::HelpCommand(DebugCommand *commands) :
        input(), commands(commands) {
    command = clipp::group(clipp::command("help", "h"));
    parser = (
        clipp::group(command),
        clipp::opt_values("command").set(input)
    );
    command.doc("Show usage of debugger commands");
    command.push_back(clipp::any_other(extraArgs));
}


void HelpCommand::parse(std::vector<std::string> &tokens, ParsingResult &res) {
    DebugCommandObject::parse(tokens, res);
    if (!res.success) input.clear();
}

void HelpCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    ParsingResult pRes;
    pRes.command = nullptr;
    if (input.empty()) {
        res.setMsg("COMMANDS\n" + commands->usageString());
        return;
    }
    commands->parse(input, pRes);
    if (!pRes.command) {
        res.setMsg("COMMANDS\n" + commands->usageString());
        return;
    }

    res.setMsg(pRes.command->usageString());
    input.clear();
}

};
