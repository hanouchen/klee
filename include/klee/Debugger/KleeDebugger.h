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
class KInstruction;

namespace {
const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}

class KDebugger {
public:

    KDebugger();

    void selectState();
    void selectBranch(int, std::string &);

    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void checkBreakpoint(ExecutionState &state);
    void setStatsTracker(StatsTracker *tracker) { this->statsTracker = tracker; }
    void setSearcher(DebugSearcher *searcher);
    void showPromptAtInstruction(const KInstruction *);
    void handleCommand(std::vector<std::string> &, std::string &);
    const std::set<Breakpoint> &breakpoints();


private:

    Prompt prompt;
    DebugSearcher *searcher;
    StatsTracker *statsTracker;
    std::set<Breakpoint> m_breakpoints;
    bool step;

    void handleContinue();
    void handleRun();
    void handleStep();
    void handleQuit();
    void handleHelp();
    void handleBreakpoint(std::string &);
    void handleInfo(InfoOpt opt);
    void printStats();
    void handleState(StateOpt opt, std::string &);
    void printBreakpoints();
    void printStack(ExecutionState *);
    void printConstraints(ExecutionState *);
};
}
#endif