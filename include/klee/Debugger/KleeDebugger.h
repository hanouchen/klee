#ifndef KLEE_DEBUGGER_H
#define KLEE_DEBUGGER_H

#include <vector>
#include <string>
#include <unordered_map>

#include "../../../lib/Core/Searcher.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/ExecutionState.h"
#include "klee/Debugger/Breakpoint.h"
#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/Prompt.h"

#include "llvm/IR/Module.h"

namespace klee {
class DebugSearcher;
class StatsTracker;
struct KInstruction;
class Executor;

class DebugInfoTable;

class KDebugger {
public:
    ~KDebugger();
    KDebugger();

    void init();
    void preprocess();
    void onError();
    int showPrompt();
    int checkBreakpoint(ExecutionState &state);

    void setStatsTracker(StatsTracker *tracker) { this->statsTracker = tracker; }
    void setExecutor(Executor *executor) { this->executor = executor; }
    void setModule(llvm::Module *module) { this->module = module; }
    void setDbgSearcher(DbgSearcher *searcher) { this->dbgSearcher = searcher; }
    void setStopUponBranching(bool val) { this->stopUponBranching = val; }
    void setStepping() { step = true; }
    void setSteppingInstruction() { stepi = true; }

private:
    Prompt prompt;
    DebugCommandList *commands;
    Executor *executor;
    DbgSearcher *dbgSearcher;
    StatsTracker *statsTracker;
    llvm::Module *module;
    std::vector<Breakpoint> breakpoints;
    bool step;
    bool stepi;
    bool stopUponBranching;
    bool stopOnError;
    PrintStateOption printStateOpt;

    int alertBranching(bool askForSelection);
    void selectBranch(int);
};
}
#endif
