#include <algorithm>
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"

#include "klee/Debugger/clipp.h"
#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/InfoCommand.h"
#include "../lib/Core/Searcher.h"
#include "../Core/StatsTracker.h"
#include "klee/Debugger/linenoise.h"

namespace klee {

InfoCommand::InfoCommand() {
    command = (
        clipp::command("info").doc("Show various execution informations"),
        clipp::any_other(extraArgs));
}

InfoBreakpointCommand::InfoBreakpointCommand(std::vector<Breakpoint> *bps) :
        breakpoints(bps), type(BreakpointType::breakpoint) {

    command = (
        clipp::command("info"),
        clipp::one_of(clipp::command("breakpoint").
                      set(type, BreakpointType::breakpoint),
                      clipp::command("killpoint").
                      set(type, BreakpointType::killpoint)),
        clipp::any_other(extraArgs)
    ).doc("List all breakpoints or killpoints");
    parser = command;
}

void InfoBreakpointCommand::execute(CommandResult &res) {
    std::string msg = {};
    const unsigned locationCol = 9;
    llvm::raw_string_ostream ss(msg);
    llvm::formatted_raw_ostream fo(ss);
    fo << "Num";
    fo.PadToColumn(locationCol);
    fo << "Location\n";
    for (auto & bp : *breakpoints) {
        if (bp.type != type) {
            continue;
        }
        fo << bp.idx;
        fo.PadToColumn(locationCol);
        fo << bp.file << ":" << bp.line << "\n";
    }
    fo.flush();
    ss.flush();
    msg = ss.str();
    res.success = true;
    res.stayInDebugger = true;
    res.setMsg(msg);
}


InfoStackCommand::InfoStackCommand(DbgSearcher *searcher) :
        searcher(searcher) {
    command = (
        clipp::command("info"),
        clipp::command("stack"),
        clipp::any_other(extraArgs)
    ).doc("Print the stack dump of current state");
    parser = command;
}


void InfoStackCommand::execute(CommandResult &res) {
    auto state = searcher->currentState();
    res.stayInDebugger = true;
    res.success = true;
    debugutil::printStack(state);
}

InfoConstraintsCommand::InfoConstraintsCommand(DbgSearcher *searcher) :
        searcher(searcher) {
    command = (
        clipp::command("info"),
        clipp::command("constraints"),
        clipp::any_other(extraArgs)
    ).doc("Print constraints of the current state");
    parser = command;
}

void InfoConstraintsCommand::execute(CommandResult &res) {
    auto state = searcher->currentState();
    res.stayInDebugger = true;
    res.success = true;
    debugutil::printConstraints(state);
}

InfoStateCommand::InfoStateCommand(DbgSearcher *searcher) :
        searcher(searcher) {
    command = (
        clipp::command("info"),
        clipp::command("state"),
        clipp::any_other(extraArgs)
    ).doc("Print information of the current state");
    parser = command;
}

void InfoStateCommand::execute(CommandResult &res) {
    auto state = searcher->currentState();
    res.stayInDebugger = true;
    res.success = true;
    debugutil::printState(state);
}

InfoStatesCommand::InfoStatesCommand(DbgSearcher *searcher) :
        searcher(searcher), opt(PrintStateOption::DEFAULT) {
    command = (
        clipp::command("info"),
        clipp::command("states")
    );

    parser = (
        clipp::group(command),
        clipp::option("--compact", "-c").set(opt, PrintStateOption::COMPACT).doc("Use compact state representation"),
        clipp::any_other(extraArgs)
    );

    command.doc("Print information of all exeucution states");
    command.push_back(clipp::any_other(extraArgs));
}

void InfoStatesCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    res.success = true;
    auto statesSet = searcher->getStates();
    std::vector<ExecutionState *> states(statesSet.begin(), statesSet.end());
    int total = states.size();
    llvm::outs().changeColor(llvm::raw_ostream::GREEN);
    llvm::outs() << "Total number of states: " << total;
    llvm::outs() << "\n";

    if (total > 3) opt = PrintStateOption::COMPACT;
    std::string str;
    llvm::raw_string_ostream ss(str);
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    llvm::outs() << "Page 1:" << "\n\n";
    for (int i = 0; i < std::min(total, 15); ++i) {
        if (states[i] == searcher->currentState()) {
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "current state --> ";
            llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        }
        debugutil::printState(states[i], opt);
        llvm::outs() << "\n";
    }

    if (total > 15) {
        displayLongList(states);
    }

    res.setMsg("");
    opt = PrintStateOption::DEFAULT;
}

void InfoStatesCommand::displayLongList(std::vector<ExecutionState *> &states) {
    char *cmd;
    char msg[] = "(\"p\": next page, \"q\": previous page, \"q\": quit) ";
    int pageNumber = 0;
    int itemsPerPage = 15;
    int pages = (states.size() - 1) / itemsPerPage + 1;

    while ((cmd = linenoise(msg)) != NULL) {
        std::string s(cmd);
        if (s == "n") {
            pageNumber++;
            if (pageNumber == pages) {
                break;
            }
        } else if (s == "p") {
            pageNumber--;
            if (pageNumber < 0) {
                break;
            }
        } else if (s == "q") {
            break;
        } else {
            linenoiseFree(cmd);
            continue;
        }

        llvm::outs() << "Page " << (1 + pageNumber) << "\n\n";
        for (int i = 0; i < itemsPerPage; ++i) {
            if (i + pageNumber * itemsPerPage >= (int)states.size()) {
                break;
            }
            auto state = states[itemsPerPage * pageNumber + i];
            if (state == searcher->currentState()) {
                llvm::outs().changeColor(llvm::raw_ostream::GREEN);
                llvm::outs() << "current state --> ";
                llvm::outs().changeColor(llvm::raw_ostream::WHITE);
            }
            debugutil::printState(state, PrintStateOption::COMPACT);
            llvm::outs() << "\n";
        }
        linenoiseFree(cmd);
    }

    linenoiseFree(cmd);

}

InfoStatsCommand::InfoStatsCommand(StatsTracker *statsTracker) :
        statsTracker(statsTracker) {
    command = (
        clipp::command("info"),
        clipp::command("stats"),
        clipp::any_other(extraArgs)
    ).doc("Print statistics");
    parser = command;
}

void InfoStatsCommand::execute(CommandResult &res) {
    std::string msg = {};
    llvm::raw_string_ostream ss(msg);
    res.stayInDebugger = true;
    res.success = true;
    statsTracker->printStats(ss);
    ss.flush();
    res.setMsg(ss.str());
}


};
