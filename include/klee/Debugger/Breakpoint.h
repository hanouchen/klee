#ifndef KLEE_BREAKPOINT_H
#define KLEE_BREAKPOINT_H

#include <string>

struct Breakpoint {
    std::string file;
    unsigned line;

    Breakpoint() : file(""), line(0) {}
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

#endif