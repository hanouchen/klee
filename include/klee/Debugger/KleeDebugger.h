#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <set>
#include <string>

#include "klee/Internal/Module/KInstruction.h"

namespace klee {

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
    void showPrompt();
    void checkBreakpoint(const KInstruction *ki);
    bool quitKlee();
    const std::set<Breakpoint> &breakpoints();

private:
    std::set<Breakpoint> m_breakpoints;
    bool m_quitKlee;

    void printHelp();
    void printBreakpoints();
    void addBreakpointFromLine(const char *line);
};
}
#endif