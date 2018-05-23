#ifndef KLEE_DEBUGUTIL_H
#define KLEE_DEBUGUTIL_H

#include <vector>
#include <string>
#include "klee/Debugger/DebugCommand.h"

namespace klee {
class ExecutionState;
class KInstIterator;

enum class PrintStateOption {
    DEFAULT,
    COMPACT
};

enum class PrintCodeOption {
    DEFAULT, /* loc, source, assembly */
    LOC_SOURCE, /* loc, source */
    SOURCE_ONLY, /* source */
};

namespace debugutil {

std::string getSourceLine(const std::string &file, int from, int to = -1);
std::string getFileFromPath(const std::string &fullPath);
void printState(ExecutionState *,
                PrintStateOption option = PrintStateOption::DEFAULT,
                llvm::raw_ostream &os = llvm::outs());
void printStack(ExecutionState *, llvm::raw_ostream &os = llvm::outs());
void printCode(KInstIterator ki,
               PrintCodeOption option = PrintCodeOption::DEFAULT,
               llvm::raw_ostream &os = llvm::outs());
void printConstraints(ExecutionState *, llvm::raw_ostream &os = llvm::outs());

};

};


#endif
