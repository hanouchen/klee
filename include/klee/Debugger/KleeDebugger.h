#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <set>
#include <string>

#include "klee/Internal/Module/KInstruction.h"
#include "klee/ExecutionState.h"
#include "klee/Debugger/Breakpoint.h"

namespace klee {
class DebugSearcher;

namespace {
const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}

class KDebugger{
public:
    KDebugger();
    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void showPromptAtBreakpoint(const Breakpoint &breakpoint);
    void checkBreakpoint(ExecutionState &state);
    bool stateChanged();
    const std::set<Breakpoint> &breakpoints();
    DebugSearcher *getSearcher() { return m_searcher; }

private:

    // Set of breakpoints set by the user
    std::set<Breakpoint> m_breakpoints;
    DebugSearcher *m_searcher;

    void printHelp();
    void printBreakpoints();
    void addBreakpointFromLine(const char *line);
    void printStack();
    void printConstraints();
    void printStateInfo(const char *line);
    void nextState();
};
}
#endif