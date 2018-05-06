#ifndef KLEE_BREAKPOINT_H
#define KLEE_BREAKPOINT_H

#include <string>

typedef enum class BreakpointType {
    breakpoint,
    killpoint
} PType;

struct Breakpoint {
    static unsigned cnt;

    BreakpointType type;
    std::string file;
    unsigned line;
    unsigned idx;

    Breakpoint() : type(BreakpointType::breakpoint), file(""), line(0), idx(0) {}
    Breakpoint(BreakpointType type, const std::string &file, unsigned line)
        : type(type), file(file), line(line), idx(cnt) {}

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

#endif