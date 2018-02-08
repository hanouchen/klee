#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <set>
#include <string>

#include "../../../lib/Core/Searcher.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/ExecutionState.h"
#include "klee/Debugger/Breakpoint.h"
#include "klee/Debugger/Command.h"
#include "klee/Debugger/Prompt.h"

namespace klee {
class DebugSearcher;
class StatsTracker;

namespace {
const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}

class KDebugger {
public:

    KDebugger();

    void selectState();

    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void showPromptAtBreakpoint(const Breakpoint &breakpoint);
    void checkBreakpoint(ExecutionState &state);
    void setStatsTracker(StatsTracker *tracker) { this->statsTracker = tracker; }
    void setSearcher(DebugSearcher *searcher);
    const std::set<Breakpoint> &breakpoints();


private:

    Prompt prompt;
    DebugSearcher *searcher;
    StatsTracker *statsTracker;
    std::set<Breakpoint> m_breakpoints;
    bool initialPrompt;

    void handleCommand(std::vector<std::string> &);
    void handleContinue();
    void handleRun();
    void handleQuit();
    void handleHelp();
    void handleBreakpoint(std::string &);
    void handleInfo(InfoOpt opt);
    void printStats();
    void handleState(StateOpt opt);
    void printBreakpoints();
    void printStack();
    void printConstraints();
};
}
#endif