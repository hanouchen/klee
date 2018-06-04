#ifndef KLEE_SETCOMMAND_H
#define KLEE_SETCOMMAND_H

#include <string>
#include "klee/Debugger/DebugCommand.h"
#include "../../../lib/Core/Executor.h"
#include "../../../lib/Core/Searcher.h"

namespace klee {


class SetSymbolicCommand : public DebugCommandObject {
public:
    SetSymbolicCommand(Executor *executor, DbgSearcher *searcher);
    virtual std::string getName() { return "set-symbolic"; }
    virtual void execute(CommandResult &res);

private:
    std::string var;
    Executor *executor;
    DbgSearcher *searcher;
};

};

#endif