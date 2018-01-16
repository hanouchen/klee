#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <set>
#include <string>

#include "klee/Internal/Module/KInstruction.h"
#include "klee/ExecutionState.h"

namespace klee {

namespace {
const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}

struct Breakpoint {
    std::string file;
    unsigned line;

    Breakpoint(const std::string &file, unsigned line) : file(file), line(line) {}

    bool operator<(const Breakpoint& rhs) const {
        return (file < rhs.file) || (file == rhs.file && line < rhs.line);
    }

    bool operator==(const Breakpoint& rhs) const {
        return file == rhs.file && line == rhs.line;
    }

    bool operator!=(const Breakpoint& rhs) const {
        return !(*this == rhs);
    }
};

class KDebugger{
public:
    KDebugger();
    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void showPromptAtBreakpoint(const Breakpoint &breakpoint);
    void checkBreakpoint(const ExecutionState &state);
    bool quitKlee();
    const std::set<Breakpoint> &breakpoints();

private:
    // The last breakpoint encountered, used to prevent stopping at the same source line
    // multiple times (since one source line can correspond to multiple assembly lines)
    Breakpoint m_prevBp;

    // Execution state at the current breakpoint, initially null.
    const ExecutionState *m_currentState;

    // Set of breakpoints set by the user
    std::set<Breakpoint> m_breakpoints;

    void printHelp();
    void printBreakpoints();
    void addBreakpointFromLine(const char *line);
    void printStack();
    void printConstraints();
    void printStateInfo(const char *line);
};
}
#endif