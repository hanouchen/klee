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
        return file == file && line == line;
    }
};

class KDebugger{
public:
    void showPrompt(const char *prompt = DEFAULT_PROMPT);
    void showPromptAtBreakpoint(const Breakpoint &breakpoint);
    void checkBreakpoint(const ExecutionState &state);
    bool quitKlee();
    const std::set<Breakpoint> &breakpoints();

private:
    const ExecutionState *m_currentState;
    std::set<Breakpoint> m_breakpoints;
    bool m_quitKlee;

    void printHelp();
    void printBreakpoints();
    void addBreakpointFromLine(const char *line);
    void showExecutionStateInformation();
};
}
#endif