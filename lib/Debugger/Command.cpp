#include <klee/Debugger/Command.h>
#include <klee/Debugger/clipp.h>
#include "llvm/Support/raw_ostream.h"

namespace klee {

namespace debugcommands {

using namespace clipp;
CommandType selected = CommandType::none;

// Options for commands
InfoOpt infoOpt = InfoOpt::state;
StateOpt stateOpt = StateOpt::invalid;
TerminateOpt termOpt = TerminateOpt::current;
GenerateInputOpt generateInputOpt = GenerateInputOpt::char_array;

// String for breakpoint in the form of <file:line>
std::string bpString = "";

// String for print command to store variable name
std::string var = "";

// Any extra (invalid) arguments for a command
std::vector<std::string> extraArgs;

std::string stateAddrHex = "";

// The index of the state that user chooses to continue
// execution on when execution branches.
bool stopUponBranching = false;
int stateIdx = 0;
unsigned breakpointIdx = 0;

group continueCmd = (
    command("c", "continue").set(selected, CommandType::cont).
    doc("Continue execution until the next breakpoint (or end of program)"),
    one_of(
        option("-s").set(stopUponBranching, true).doc("stop when execution branches")).
        doc("continue options:"),
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

group deleteCmd = (
    command("d", "del").set(selected, CommandType::del).
    doc("Delete a breakpoint"),
    group(value("breakpoint number", breakpointIdx).
    doc("Number of the breakpoint to delete\n").
    if_missing([] { 
        if (selected == CommandType::breakpoint) {
            llvm::outs() << "Please enter a file name and a line to break.\n";
        }
    })).doc("Delete argument:"),
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

group setCmd = (
    command("set").set(selected, CommandType::set).
    doc("Set the value of a variable"),
    group(value("var", var).
    doc("The variable to set\n").
    if_missing([] { 
        if (selected == CommandType::set) {
            llvm::outs() << "Please enter a variable name\n";
        }
    })).doc("set argument:"),
    any_other(extraArgs));

group infoCmd = (
    command("info").set(selected, CommandType::info).doc("Print info"),
    one_of(
        option("break").set(infoOpt, InfoOpt::breakpoints).
        doc("List all breakpoints"),
        option("states").set(infoOpt, InfoOpt::states).
        doc("List all states"),
        option("stack").set(infoOpt, InfoOpt::stack).
        doc("Dump the stack of the current execution state"),
        option("constraints").set(infoOpt, InfoOpt::constraints).
        doc("Print the constraints of the current execution state"),
        option("stats").set(infoOpt, InfoOpt::statistics).
        doc("Print the execution statistics of klee so far"),
        option("state").set(infoOpt, InfoOpt::state).
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
        doc("Move to the previous state"),
        value("address").set(stateAddrHex).
        doc("Address of the state to move to\n").
        if_missing([]{ 
            if (selected == CommandType::state) {
                llvm::outs() << "Please specify a direction.\n";
            }
        })).doc("state options:")),
    any_other(extraArgs));

group terminateCmd = ((
    command("terminate").set(selected, CommandType::terminate).
    doc("Terminate execution state(s)"),
    one_of(
        option("current").set(termOpt, TerminateOpt::current).
        doc("Terminal current execution state"),
        option("other").set(termOpt, TerminateOpt::other).
        doc("Terminal all but current execution state\n")
    ).doc("terminate options:"),
    any_other(extraArgs)));

group generateConcreteInputCmd = ((
    command("generate-input").set(selected, CommandType::generate_input).
    doc("Generate concrete input values for the current state"),
    one_of(
        option("int").set(generateInputOpt, GenerateInputOpt::integer).
        doc("Show the concrete values in integers (not implemented) \n"),
        option("hex-char").set(generateInputOpt, GenerateInputOpt::char_array)
    ).doc("generate-input options:"),
    any_other(extraArgs)));
    
group cmds[13] = {
    continueCmd,
    runCmd,
    stepCmd,
    quitCmd,
    helpCmd,
    breakCmd,
    deleteCmd,
    printCmd,
    setCmd,
    infoCmd,
    stateCmd,
    terminateCmd,
    generateConcreteInputCmd
};

group cmdParser = one_of(
    continueCmd, 
    runCmd,
    stepCmd,
    quitCmd,
    helpCmd,
    breakCmd,
    deleteCmd,
    printCmd,
    infoCmd,
    stateCmd,
    terminateCmd,
    generateConcreteInputCmd
);

group branchSelection = (
    value("State Number", stateIdx),
    any_other(extraArgs)
);

}

}