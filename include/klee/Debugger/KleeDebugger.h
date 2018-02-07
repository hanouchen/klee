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

namespace {
const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}

class KDebugger : public Searcher {
public:

    KDebugger();

    bool empty();
    ExecutionState &selectState();
    void update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates);

    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void showPromptAtBreakpoint(const Breakpoint &breakpoint);
    void checkBreakpoint(ExecutionState &state);
    const std::set<Breakpoint> &breakpoints();
    DebugSearcher *getSearcher() { return m_searcher; }

    void handleCommand(CommandBuffer &cmdBuff);
    void handleContinue(CommandBuffer &);
    void handleRun(CommandBuffer &);
    void handleQuit(CommandBuffer &);
    void handleBreakpoint(CommandBuffer &);
    void handleInfo(CommandBuffer &);
    void handleHelp(CommandBuffer &);
    void handleState(CommandBuffer &);

private:

    // Set of breakpoints set by the user
    Prompt prompt;
    std::set<Breakpoint> m_breakpoints;
    DebugSearcher *m_searcher;
    bool initialPrompt;

    void printBreakpoints();
    void printStack();
    void printConstraints();
    void nextState();
};
}
#endif