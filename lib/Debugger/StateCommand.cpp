#include <string>

#include "llvm/Support/Format.h"
#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/StateCommand.h"
#include "klee/Debugger/clipp.h"
#include "../lib/Core/Executor.h"
#include "../lib/Core/Searcher.h"

namespace klee {

StateCommand::StateCommand() {
 command = (
        clipp::command("state").doc("Manipulate execution state"),
        clipp::any_other(extraArgs));
}

StateCycleCommand::StateCycleCommand(DbgSearcher *searcher) :
        forward(false), searcher(searcher) {
    command = (
        clipp::command("state"),
        clipp::one_of(
            clipp::command("prev").set(forward, false),
            clipp::command("next").set(forward, true)
        ),
        clipp::any_other(extraArgs)
    ).doc("Move to the next(or previous) state");
    parser = command;
}

void StateCycleCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    res.success = true;
    res.msg = {};
    if (forward) searcher->nextState();
    auto *state = searcher->currentState();
    std::stringstream ss;
    ss << std::hex << state;
    res.setMsg("Moved to state @" + ss.str() + "\n");
}

StateMoveCommand::StateMoveCommand(DbgSearcher *searcher) :
        addressStr(), searcher(searcher) {
    command = (
        clipp::command("state"),
        clipp::value("address").set(addressStr),
        clipp::any_other(extraArgs)
    ).doc("Move to state at given address");
    parser = command;
}

void StateMoveCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    res.success = false;
    res.msg = {};
    if (addressStr.size()) {
        char *r = 0;
        if (addressStr.size() >= 2 && addressStr[0] == '0' && addressStr[1] == 'x') {
            addressStr = addressStr.substr(2);
        }
        unsigned long long address = strtoul(addressStr.c_str(), &r, 16);
        if (*r != 0) {
            res.setMsg("Please enter a valid address (hex number) \n");
            res.success = false;
            return;
        } else {
            searcher->setStateAtAddr(address);
            auto *state = searcher->currentState();
            std::stringstream ss;
            ss << std::hex << state;
            res.setMsg("Moved to state @" + ss.str() + "\n");
        }
        addressStr = "";
    }
    res.success = true;
}

ToggleCompactCommand::ToggleCompactCommand(PrintStateOption *opt) :
        opt(opt) {
    command = (
        clipp::command("toggle"),
        clipp::command("compact"),
        clipp::any_other(extraArgs)
    ).doc("Toggle compact represetation of states");
    parser = command;
    *opt = PrintStateOption::DEFAULT;
}

void ToggleCompactCommand::execute(CommandResult &res) {
    std::string rep = *opt == PrintStateOption::DEFAULT ? "compact" : "default";
    *opt = *opt == PrintStateOption::DEFAULT ? PrintStateOption::COMPACT
                                             : PrintStateOption::DEFAULT;
    res.setMsg("Now using " + rep + " representation\n");
    res.stayInDebugger = true;
}

TerminateCommand::TerminateCommand(Executor *executor, DbgSearcher *searcher) :
        terminateCurrent(true), executor(executor), searcher(searcher) {
    command = clipp::group(clipp::command("terminate", "t"));
    parser = (
        clipp::group(command),
        clipp::option("--others", "-o").set(terminateCurrent, false).doc("Terminate all but the current state"),
        clipp::any_other(extraArgs)
    );
    command.doc("Terminate the specified execution state(s)");
    command.push_back(clipp::any_other(extraArgs));
}

void TerminateCommand::execute(CommandResult &res) {
    res.success = true;
    res.stayInDebugger = true;
    res.setMsg("");
    llvm::Twine msg("");
    if (terminateCurrent) {
        executor->terminateState(*searcher->currentState());
        executor->updateStates(nullptr);
        searcher->updateCurrentState();
        if (searcher->getStates().size() == 0) {
            res.stayInDebugger = false;
        }
    } else {
        res.stayInDebugger = true;
        for (auto state : searcher->getStates()) {
            if (state != searcher->currentState()) {
                executor->terminateStateEarly(*state, msg);
            }
        }
        executor->updateStates(nullptr);
        searcher->updateCurrentState();
    }
    terminateCurrent = true;
}

GenerateInputCommand::GenerateInputCommand(Executor *executor, DbgSearcher *searcher) :
        executor(executor), searcher(searcher) {
    command = (clipp::command("generate-input"), clipp::any_other(extraArgs));
    parser = command;
    command.doc("Generate input that leads to the current execution state");
}

void GenerateInputCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    res.setMsg("");

    debugutil::printConstraints(searcher->currentState());

    std::vector<std::pair<std::string, std::vector<unsigned char>>> out;
    res.success = executor->getSymbolicSolution(*searcher->currentState(), out);

    if (!res.success) {
        llvm::outs().changeColor(llvm::raw_ostream::RED);
        llvm::outs() << "No satisfying input values found\n";
        llvm::outs().changeColor(llvm::raw_ostream::WHITE);
        return;
    }

    if (!out.size()) {
        return;
    }

    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "\nConcrete input value(s) that will reach this path:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);

    for (auto &p : out) {
      llvm::outs() << p.first << " (size " << p.second.size() << "): ";
      for (auto c : p.second) {
        llvm::outs() << llvm::format("\\0x%02x", int(c));
      }
      llvm::outs() << "\n";
    }
}

};