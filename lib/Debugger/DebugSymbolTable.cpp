
#include "klee/Debugger/DebugSymbolTable.h"
#include <string>
#include <unordered_map>

#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <klee/Internal/Module/Cell.h>
#include <klee/Expr.h>
#include <klee/ExecutionState.h>
#include <klee/Internal/Module/Kmodule.h>
#include <klee/Internal/Module/KInstruction.h>

namespace klee {

bool DebugSymbolTable::bindAddress(std::string &symbol, llvm::Value *address, StackFrame &sf) {
    auto it = table.find(symbol);
    if (it != table.end() && it->second.hasAddress) {
        llvm::outs() << "Symbol " << symbol << " already bound to an address.\n";
        return false;
    }
    SymbolValue value;
    SymbolValue &sv = (it != table.end()) ? it->second : value;
    if (llvm::AllocaInst *alloca = dyn_cast<llvm::AllocaInst>(address)) {
        sv.hasAddress = true;
        sv.type = alloca->getAllocatedType();
        auto kf = sf.kf;
        for (unsigned j = 0; j < kf->numInstructions; ++j) {
            auto inst = kf->instructions[j]->inst;
            if (alloca == inst) {
                int vnumber = kf->instructions[j]->dest;
                assert(vnumber > 0 && "Invalid address, not a value or constant!");
                sv.address = sf.locals[vnumber].value;
                llvm::outs() << "Binding address " << sv.address << " to symbol " << symbol << "\n";
                table[symbol] = sv;
                return true;
            }
        }
    }
    return false;
}

SymbolValue *DebugSymbolTable::lookup(std::string &symbol) {
    return table.find(symbol) == table.end() ? nullptr : &table[symbol];
}

}