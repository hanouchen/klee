#include <assert.h>
#include <iostream>
#include <functional>
#include <regex>
#include <sstream>
#include <vector>

// FIXME: Probably not a good idea?
#include "../Core/Searcher.h"
#include "../Core/StatsTracker.h"
#include "llvm/Support/raw_ostream.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Statistics.h"

namespace klee {

namespace {

using namespace clipp;
enum class commandtype {cont, run, quit, breakpoint, info, help, state, none};
CommandType selected = CommandType::none;
InfoOpt infoOpt = InfoOpt::all;
StateOpt stateOpt = StateOpt::invalid;
std::string bpString;

auto contcmd = (command("c").set(selected, CommandType::cont));
auto runcmd = (command("r").set(selected, CommandType::run));
auto quitcmd = (command("q").set(selected, CommandType::quit));
auto help = (command("h").set(selected, CommandType::help));
auto bcmd = (command("b").set(selected, CommandType::breakpoint), 
             values("", bpString));
auto info = (command("info").set(selected, CommandType::info),
             one_of(option("breakpoints").set(infoOpt, InfoOpt::breakpoints),
                    option("stack").set(infoOpt, InfoOpt::stack),
                    option("constraints").set(infoOpt, InfoOpt::constraints),
                    option("stats").set(infoOpt, InfoOpt::statistics),
                    option("all").set(infoOpt, InfoOpt::all)));

auto changestate = (command("state").set(selected, CommandType::state),
             one_of(command("next").set(stateOpt, StateOpt::next),
                    command("prev").set(stateOpt, StateOpt::prev)));

auto cli = one_of(contcmd , runcmd , quitcmd , bcmd , help , info, changestate);

}

extern "C" void set_halt_execution(bool);
extern "C" bool klee_interrupted();

KDebugger::KDebugger() : 
    prompt(std::bind(&KDebugger::handleCommand, this, std::placeholders::_1)),
    searcher(0),
    statsTracker(0),
    m_breakpoints(), 
    initialPrompt(true) {}

void KDebugger::selectState() {
    if (klee_interrupted()) {
        set_halt_execution(false);
        int rc = prompt.show("KLEE: ctrl-c detected, execution interrupted> ");
        if (rc) {
            set_halt_execution(true);
        }
    } else {
        checkBreakpoint(*searcher->currentState());
    }
}

void KDebugger::showPrompt(const char *prompt) {
    assert(prompt);
    this->prompt.show(prompt);
}

void KDebugger::showPromptAtBreakpoint(const Breakpoint &breakpoint) {
    std::string promptMsg = breakpoint.file + ", line " + std::to_string((int)breakpoint.line) + "> ";
    int rc = prompt.show(promptMsg.c_str());
    if (rc) {
        set_halt_execution(true);
    }
}

void KDebugger::checkBreakpoint(ExecutionState &state) {
    if (m_breakpoints.empty() || !state.pc) {
        return;
    }
    auto ki = state.pc;
    std::regex fileRegex(".*\\/(\\w*\\.\\w*)");
    std::cmatch matches;
    if (std::regex_search(ki->info->file.c_str(), matches, fileRegex)) {
      Breakpoint bp(matches[1].str(), ki->info->line);
      if (state.lastBreakpoint != bp && m_breakpoints.find(bp) != m_breakpoints.end()) {
        state.lastBreakpoint = bp;
        showPromptAtBreakpoint(bp);
      }
    }
}


const std::set<Breakpoint> & KDebugger::breakpoints() {
    return m_breakpoints;
}

void KDebugger::setSearcher(DebugSearcher *searcher) {
    this->searcher = searcher;
    this->searcher->setPreSelectCallback(
        std::bind(&KDebugger::selectState, this));
}

void KDebugger::handleCommand(std::vector<std::string> &input) {
    auto res = clipp::parse(input, cli);
    if (res) {
        switch(selected) {
            case CommandType::cont:
                handleContinue();
                break;
            case CommandType::run:
                handleRun();
                break;
            case CommandType::quit:
                handleQuit();
                break;
            case CommandType::breakpoint:
                handleBreakpoint(bpString);
                break;
            case CommandType::info:
                handleInfo(infoOpt);
                break;
            case CommandType::state:
                handleState(stateOpt);
                break;
            case CommandType::help:
                handleHelp();
                break;
            default:
                llvm::outs() << "invalid\n";
        }
    }  else {
        if (res.begin()->blocked()) {
            llvm::outs() << "Unkown command: " << res.begin()->arg() << "\n";
        } else {
            llvm::outs() << res.begin()->arg() << ": ";
            for(const auto& m : res) {
                if (m.blocked() && m.conflict()) {
                    llvm::outs() << "invalid argument(s)\n";
                    return;
                } else if (m.blocked()) {
                    llvm::outs() << "too many arguments\n";
                    return;
                }
            } 
            llvm::outs() << "argument required\n";
        }
    }
}

void KDebugger::handleContinue() {
    prompt.breakFromLoop();
}

void KDebugger::handleRun() {
    m_breakpoints.clear();
    prompt.breakFromLoop();
}

void KDebugger::handleQuit() {
    set_halt_execution(true);
    prompt.breakFromLoop();
}

void KDebugger::handleBreakpoint(std::string &input) {
    std::regex breakpointRegex("(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    if (std::regex_search(input.c_str(), matches, breakpointRegex)) {
        auto res = m_breakpoints.emplace(matches.str(1), (unsigned)stoi(matches.str(2)));
        klee_message(res.second ? "Breakpoint set" : "Breakpoint already exist");
    } else {
        klee_message("Invalid breakpoint format");
    }
}

void KDebugger::handleInfo(InfoOpt opt) {
    switch (opt) {
        case InfoOpt::stack: printStack(); break;
        case InfoOpt::constraints: printConstraints(); break;
        case InfoOpt::breakpoints: printBreakpoints(); break;
        case InfoOpt::statistics: this->statsTracker->printStats(llvm::outs()); break;
        case InfoOpt::all:
            printStack();
            llvm::outs() << "\n";
            printConstraints();
            break;
        default:
            llvm::outs() << "Invalid info option, type \"h\" for help.\n";
    }
}

void KDebugger::handleState(StateOpt dir) {
    searcher->nextIter();
}

void KDebugger::printStats() {}

void KDebugger::handleHelp() {
    klee_message("\n  Type r to run the program.\n"
                 "  Type q to quit klee.\n"
                 "  Type c to continue until the next breakpoint.\n"
                 "  Type b <filename>:<linenumber> to add a breakpoint.\n"
                 "  Type info [stack | constraints] to show information about the current execution state.\n"
                 "  Type state [prev | next] to traverse available states.\n");
}

void KDebugger::printBreakpoints() {
    klee_message("%zu breakpoints set", m_breakpoints.size());
    for (auto & bp : m_breakpoints) {
        klee_message("%s at line %u", bp.file.c_str(), bp.line);
    }
}

void KDebugger::printStack() {
    auto state = searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(llvm::outs());
}

void KDebugger::printConstraints() {
    auto state = searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Constraints for current state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), state->constraints);
}

}