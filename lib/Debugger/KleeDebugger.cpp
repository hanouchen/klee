#include <assert.h>
#include <iostream>
#include <functional>
#include <regex>
#include <sstream>
#include <vector>

// FIXME: Probably not a good idea?
#include "../Core/Searcher.h"
#include "llvm/Support/raw_ostream.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Internal/Support/ErrorHandling.h"


namespace klee {

namespace {
enum Option {
    // info options
    stack,
    constraints,
    all,
    breakpoints,

    // state navigation option
    next,
    prev,

    invalid
};

enum stateOption {
};

std::pair<const char *, void(KDebugger::*)(CommandBuffer &)> commandTable[] = 
{
    {"c",     &KDebugger::handleContinue},
    {"r",     &KDebugger::handleRun},
    {"q",     &KDebugger::handleQuit},
    {"b",     &KDebugger::handleBreakpoint},
    {"info",  &KDebugger::handleInfo},
    {"help",  &KDebugger::handleHelp},
    {"state", &KDebugger::handleState},
    {0,       &KDebugger::handleHelp}
};

std::pair<const char *, Option> OptStrs[] = 
{
    {"",            all},
    {"stack",       stack},
    {"constraints", constraints},
    {"breakpoints", breakpoints},

    {"next",        next},
    {"prev",        prev},

    {0,             invalid}
};
}

extern "C" void set_halt_execution(bool);
extern "C" bool klee_interrupted();

KDebugger::KDebugger() : 
    prompt(std::bind(&KDebugger::handleCommand, this, std::placeholders::_1)),
    m_breakpoints(), 
    m_searcher(new DebugSearcher()),
    initialPrompt(true) {}

ExecutionState & KDebugger::selectState() {
    if (initialPrompt) {
        prompt.show();
        initialPrompt = false;
    } else if (klee_interrupted()) {
        set_halt_execution(false);
        prompt.show("KLEE: ctrl-c detected, execution interrupted> ");
    } else {
        checkBreakpoint(m_searcher->selectState());
    }
    return m_searcher->selectState();
}

void KDebugger::update(ExecutionState *current,
                    const std::vector<ExecutionState *> &addedStates,
                    const std::vector<ExecutionState *> &removedStates) { 
    m_searcher->update(current, addedStates, removedStates);             
}

bool KDebugger::empty() {
    return m_searcher->empty();
}

void KDebugger::showPrompt(const char *prompt) {
    assert(prompt);
    this->prompt.show(prompt);
}

void KDebugger::showPromptAtBreakpoint(const Breakpoint &breakpoint) {
    std::string prompt = breakpoint.file + ", line " + std::to_string((int)breakpoint.line) + "> ";
    showPrompt(prompt.c_str());
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

void KDebugger::handleCommand(CommandBuffer &cmd) {
    auto handler = cmd.search(commandTable);
    (this->*handler)(cmd);
}

void KDebugger::handleContinue(CommandBuffer &cmd) {
    if (!cmd.endOfLine()) {
        llvm::outs() << "\"continue\" does not take arguments\n";
        return;
    }
    prompt.breakFromLoop();
}

void KDebugger::handleRun(CommandBuffer &cmd) {
    if (!cmd.endOfLine()) {
        llvm::outs() << "\"run\" does not take arguments\n";
        return;
    }
    m_breakpoints.clear();
    prompt.breakFromLoop();
}

void KDebugger::handleQuit(CommandBuffer &cmd) {
    if (!cmd.endOfLine()) {
        llvm::outs() << "\"quit\" does not take arguments\n";
        return;
    }
    set_halt_execution(true);
    prompt.breakFromLoop();
}

void KDebugger::handleBreakpoint(CommandBuffer &cmd) {
    std::regex breakpointRegex("(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    std::string input(cmd.nextToken());
    if (std::regex_search(input.c_str(), matches, breakpointRegex)) {
        auto res = m_breakpoints.emplace(matches.str(1), (unsigned)stoi(matches.str(2)));
        klee_message(res.second ? "Breakpoint set" : "Breakpoint already exist");
    } else {
        klee_message("Invalid breakpoint format");
    }
}

void KDebugger::handleInfo(CommandBuffer &cmd) {
    auto info = cmd.search(OptStrs);
    switch (info) {
        case stack: printStack(); break;
        case constraints: printConstraints(); break;
        case Option::breakpoints: printBreakpoints(); break;
        case all: 
            printStack();
            llvm::outs() << "\n";
            printConstraints();
            break;
        default:
            llvm::outs() << "Invalid info option, type \"h\" for help.\n";
    }
}

void KDebugger::handleState(CommandBuffer &cmd) {
    nextState();
}

void KDebugger::handleHelp(CommandBuffer &cmd) {
    if (!cmd.endOfLine()) {
        llvm::outs() << "\"help\" does not take arguments\n";
        return;
    }
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
    auto state = m_searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(llvm::outs());
}

void KDebugger::printConstraints() {
    auto state = m_searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Constraints for current state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), state->constraints);
}

void KDebugger::nextState() {
    m_searcher->nextIter();
}

}