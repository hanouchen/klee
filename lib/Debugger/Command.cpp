#include <klee/Debugger/Command.h>
#include <klee/Debugger/clipp.h>
#include "llvm/Support/raw_ostream.h"

namespace klee {

namespace debugcommands {

using namespace clipp;
CommandType selected = CommandType::none;

// Options for 'info' and 'state' commands
InfoOpt infoOpt = InfoOpt::all;
StateOpt stateOpt = StateOpt::invalid;

// String for breakpoint in the form of <file:line>
std::string bpString = "";

// String for print command to store variable name
std::string var = "";

// Any extra (invalid) arguments for a command
std::vector<std::string> extraArgs;

// The index of the state that user chooses to continue
// execution on when execution branches.
int stateIdx = 0;

// Whether the user wants to terminate all other states
// other than the one he/she chose.
bool terminateOtherStates = false;

group continueCmd = (
    command("c", "continue").set(selected, CommandType::cont).
    doc("Continue execution until the next breakpoint (or end of program)"),
    any_other(extraArgs));

group runCmd = (
    command("r", "run").set(selected, CommandType::run).
    doc("Continue execution until the end of program"), 
    any_other(extraArgs));
    
group stepCmd = (
    command("s", "step").set(selected, CommandType::step).
    doc("Step one assembly instruction"),
    any_other(extraArgs));

group quitCmd = (
    command("q", "quit").set(selected, CommandType::quit).
    doc("Stop execution and quit the debugger (and klee)"),
    any_other(extraArgs));

group helpCmd = (
    command("h", "help").set(selected, CommandType::help).
    doc("Print usage of klee debugger\n"),
    any_other(extraArgs));

group breakCmd = (
    command("b", "break").set(selected, CommandType::breakpoint).
    doc("Set a breakpoint"),
    group(value("file:line", bpString).
    doc("File name and line number to set the breakpoint on\n").
    if_missing([] { 
        if (selected == CommandType::breakpoint) {
            llvm::outs() << "Please enter a file name and a line to break.\n";
        }
    })).doc("break argument:"), 
    any_other(extraArgs));

group printCmd = (
    command("p", "print").set(selected, CommandType::print).
    doc("Print information about a variable"),
    group(value("var", var).
    doc("The variable to print\n").
    if_missing([] { 
        if (selected == CommandType::print) {
            llvm::outs() << "Please enter a variable name\n";
        }
    })).doc("print argument:"), 
    any_other(extraArgs));

group infoCmd = (
    command("info").set(selected, CommandType::info).doc("Print info"),
    one_of(
        option("breakpoints").set(infoOpt, InfoOpt::breakpoints).
        doc("List all breakpoints"),
        option("stack").set(infoOpt, InfoOpt::stack).
        doc("Dump the stack of the current execution state"),
        option("constraints").set(infoOpt, InfoOpt::constraints).
        doc("Print the constraints of the current execution state"),
        option("stats").set(infoOpt, InfoOpt::statistics).
        doc("Print the execution statistics of klee so far"),
        option("all").set(infoOpt, InfoOpt::all).
        doc("Default option, "
            "dumps stack and prints constraints of current state\n").
        if_conflicted([] { 
            llvm::outs() << "You can only specify one info type.";
        })).doc("info options:"),
    any_other(extraArgs));

group stateCmd = ((
    command("state").set(selected, CommandType::state).
    doc("Change the current execution state"),
    one_of(
        command("next").set(stateOpt, StateOpt::next).
        doc("Move to the next state"),
        command("prev").set(stateOpt, StateOpt::prev).
        doc("Move to the previous state\n").
        if_missing([]{ 
            if (selected == CommandType::state) {
                llvm::outs() << "Please specify a direction.\n";
            }
        })).doc("state options:")),
    any_other(extraArgs));

group cmdParser = one_of(
    continueCmd, 
    runCmd,
    stepCmd,
    quitCmd,
    helpCmd,
    breakCmd,
    printCmd,
    infoCmd,
    stateCmd
);

group branchSelection = (
    value("State Number", stateIdx),
    option("-t", "--terminate", "-T").set(terminateOtherStates),
    any_other(extraArgs)
);

}

}