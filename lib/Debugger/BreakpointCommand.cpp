#include <string>
#include <unordered_map>
#include <regex>

#include "klee/Debugger/BreakpointCommand.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/DebugUtil.h"
#include "klee/Internal/Module/KInstruction.h"

namespace klee {
StopCommand::StopCommand(std::vector<Breakpoint> *bps,
                         BreakpointType type,
                         InstructionInfoTable *infos,
                         KModule *module) :
    location(), breakpoints(bps), type(type), infos(infos), module(module)  {}

void StopCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    std::regex r("(.*\\.\\w*)\\:([0-9]+)");
    std::string typeString = type == BreakpointType::breakpoint ? "Breakpoint"
                                                                : "Killpoint";
    std::cmatch matches;
    std::string file = {};
    unsigned int line = 0;
    if (std::regex_search(location.c_str(), matches, r)) {
        file = matches.str(1);
        line = (unsigned)stoi(matches.str(2));
    } else {
        auto info = infos->getFunctionInfoByName(location);
        if (info.file == "") {
            res.setMsg("No function named " + location  + "\n");
            return;
        }
        file = debugutil::getFileFromPath(info.file);
        line = info.line;
    }

    bool fileFound = false;
    for (auto kf : module->functions) {
        for (unsigned i = 0; i < kf->numInstructions; ++i) {
            auto info = kf->instructions[i]->info;
            if (file == debugutil::getFileFromPath(info->file)) {
                fileFound = true;
                if (line == info->line) {
                    Breakpoint nbp(type, file, line);
                    kf->instructions[i]->breakPoint =
                        type == BreakpointType::breakpoint ? nbp.idx : -nbp.idx;
                    nbp.ki = kf->instructions[i];
                    auto it = std::find_if(breakpoints->begin(), breakpoints->end(), [&nbp](const Breakpoint &bp) {
                        return bp.file == nbp.file && bp.line == nbp.line;
                    });

                    if (it == breakpoints->end()) {
                        breakpoints->push_back(nbp);
                        res.setMsg(typeString + " " + std::to_string(nbp.idx) + " at " + nbp.file + ", line " + std::to_string(nbp.line) + "\n");
                        Breakpoint::cnt++;
                    } else {
                        res.success = false;
                        res.setMsg(typeString + " already exists\n");
                    }
                    return;
                }
            }
        }
    }

    if (fileFound) {
        res.setMsg("No corresponding llvm instruction for line " + std::to_string(line) + "\n");
    } else {
        res.setMsg("No source file named: " + file + "\n");
    }
    res.success = false;
}

BreakpointCommand::BreakpointCommand(std::vector<Breakpoint> *bps,
                                     InstructionInfoTable *infos,
                                     KModule *module) :
    StopCommand(bps, BreakpointType::breakpoint, infos, module) {
    command = clipp::group(clipp::command("break", "b"));

    parser = (clipp::group(command),
              clipp::value("location").set(location).doc("Location of the breakpoint, either <file:line> or <function>"),
              clipp::any_other(extraArgs)
    );
    command.doc("Set a breakpoint at the given location");
    command.push_back(clipp::any_other(extraArgs));
}

KillpointCommand::KillpointCommand(std::vector<Breakpoint> *bps,
                                   InstructionInfoTable *infos,
                                   KModule *module) :
    StopCommand(bps, BreakpointType::killpoint, infos, module) {
    command = clipp::group(clipp::command("kill", "k"));

    parser = (clipp::group(command),
              clipp::value("(file:line) or (function)").set(location).doc("Location of the killpoint, either <file:line> or <function>"),
              clipp::any_other(extraArgs)
    );
    command.doc("Terminate execution states that reach the killpoint");
    command.push_back(clipp::any_other(extraArgs));
}

DeleteCommand::DeleteCommand(std::vector<Breakpoint> *bps) :
    numbers(), location(), breakpoints(bps) {

    command = clipp::group(clipp::command("delete", "d"));

    parser = (clipp::group(command),
              clipp::opt_values("numbers", numbers).doc("Numbers of the breakpoints (killpoints) to remove"),
              clipp::any_other(extraArgs)
    );
    command.doc("Remove breakpoints (killpoints)");
    command.push_back(clipp::any_other(extraArgs));
}

void DeleteCommand::parse(std::vector<std::string> &tokens, ParsingResult &res) {
    DebugCommandObject::parse(tokens, res);
    if (!res.success) numbers.clear();
}

void DeleteCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    unsigned sz = numbers.size();
    if (sz) {
        breakpoints->erase(std::remove_if(breakpoints->begin(), breakpoints->end(), [&](const Breakpoint &bp){
            auto it = std::find(numbers.begin(), numbers.end(), bp.idx);
            if (it != numbers.end()) {
                bp.ki->breakPoint = 0;
                numbers.erase(it);
                return true;
            }
            return false;
        }), breakpoints->end());
        std::string msg = std::to_string(sz - numbers.size()) + " breakpoint(s)/killpoints(s) successfully removed.\n";
        if (numbers.size()) {
            msg += "No breakpoint(s)/killpoint(s) with the following number(s):\n";
            for (auto num : numbers) {
                msg += std::to_string(num) + " ";
            }
            msg += "\nType \"info break\" to list all breakpoints.\n";
        }
        res.setMsg(msg);
    } else {
        breakTable->clear();
        killTable->clear();
        breakpoints->clear();
        res.setMsg("All breakpoints and killpoints removed.\n");
    }
    numbers.clear();
}

}