#include <string>
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/SetCommand.h"

namespace klee {

SetSymbolicCommand::SetSymbolicCommand(Executor *executor, DbgSearcher *searcher) :
        var(), executor(executor), searcher(searcher) {
    command = clipp::group(clipp::command("set-symbolic"));
    parser = (
        clipp::group(command),
        clipp::value("variable").set(var),
        clipp::any_other(extraArgs)
    );
    command.doc("Make a variable symbolic");
    command.push_back(clipp::any_other(extraArgs));
}

void SetSymbolicCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    llvm::MDNode *md = searcher->currentState()->prevPC->inst->getMetadata("dbg");
    llvm::Value *scope = nullptr;
    if (md) {
        scope = md->getOperand(2);
    }
    auto &stack = searcher->currentState()->stack;
    auto &sf = stack.back();
    auto svalue = sf.st.lookup(var, scope);
    if (!svalue) {
        llvm::outs() << "Unable to find variable " << var << "\n";
        return;
    }
    if (svalue != nullptr && svalue->hasAddress) {
        auto state = searcher->currentState();
        auto addr = svalue->address;
        if (!isa<ConstantExpr>(addr))
            addr = state->constraints.simplifyExpr(addr);
        ObjectPair op;
        bool success;
        state->addressSpace.resolveOne(*state, executor->solver, addr, op, success);
        if (success) {
            executor->executeMakeSymbolic(*searcher->currentState(), op.first, var);
            llvm::outs() << var << " is now symbolic\n";
            return;
        }
    }

    llvm::outs() << "Unable to find variable " << var << "\n";
}

};