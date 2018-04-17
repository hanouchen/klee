#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <vector>
#include <string>

#include "../../../lib/Core/Searcher.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/ExecutionState.h"
#include "klee/Debugger/Breakpoint.h"
#include "klee/Debugger/Command.h"
#include "klee/Debugger/Prompt.h"

#include "llvm/IR/Module.h"

namespace klee {
class DebugSearcher;
class StatsTracker;
class KInstruction;
class KInstIterator;
class Executor;

class KDebugger {
public:
    KDebugger();

    void handleCommand(std::vector<std::string> &, std::string &);
    void preprocess();
    void selectBranch(int, std::string &);

    void setStatsTracker(StatsTracker *tracker) { this->statsTracker = tracker; }
    void setExecutor(Executor *executor) { this->executor = executor; }
    void setModule(llvm::Module *module) { this->module = module; }
    void setSearcher(DebugSearcher * searcher) { this->searcher = searcher; }

private:
    Prompt prompt;
    Executor *executor;
    DebugSearcher *searcher;
    StatsTracker *statsTracker;
    std::vector<Breakpoint> breakpoints;
    llvm::Module *module;
    bool step;
    static void (KDebugger::*processors[])(std::string &);

    void checkBreakpoint(ExecutionState &state);
    void showPromptAtInstruction(const KInstruction *);

    void processContinue(std::string &);
    void processRun(std::string &);
    void processStep(std::string &);
    void processQuit(std::string &);
    void processHelp(std::string &);
    void processBreakpoint(std::string &);
    void processDelete(std::string &);
    void processPrint(std::string &);
    void processInfo(std::string &);
    void processState(std::string &);
    void processTerminate(std::string &);
    void generateConcreteInput(std::string &);

    void printBreakpoints();
    void printAllStates();
    void printState(ExecutionState *);
    void printStack(ExecutionState *);
    void printCode(ExecutionState *);
    void printConstraints(ExecutionState *);
};
}
#endif