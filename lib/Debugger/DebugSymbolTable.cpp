
#include "klee/Debugger/DebugSymbolTable.h"
#include <string>
#include <unordered_map>

#include "llvm/DebugInfo.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <klee/Internal/Module/Cell.h>
#include <klee/Expr.h>
#include <klee/ExecutionState.h>
#include <klee/Internal/Module/Kmodule.h>
#include <klee/Internal/Module/KInstruction.h>

namespace klee {

DebugSymbolTable::~DebugSymbolTable() {
    destroy();
}

bool DebugSymbolTable::bindAddress(std::string &symbol, llvm::Value *address, StackFrame &sf, llvm::Value *scope) {
    if (!this->scope) {
        this->scope = scope;
    } else if (this->scope != scope) {
        if (!child) {
            child = new DebugSymbolTable();
            child->scope = scope;
            llvm::outs() << "Opening new scope\n";
        } else {
            llvm::MDNode *md = dyn_cast<llvm::MDNode>(scope);
            if (md) {
                if (md->getOperand(2) == this->scope) {
                    child->destroy();
                    child = new DebugSymbolTable();
                    child->scope = scope;
                    llvm::outs() << "Destory old scope for new one\n";
                }
            }
        }
        return child->bindAddress(symbol, address, sf, scope);
    }

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
                if (vnumber < 0) {
                    return false;
                }
                sv.address = sf.locals[vnumber].value;
                llvm::outs() << "Binding address " << sv.address << " to symbol " << symbol << "\n";
                table[symbol] = sv;
                return true;
            }
        }
    }
    return false;
}

void DebugSymbolTable::updateValue(std::string &symbol, llvm::Value *value) {
    auto it = table.find(symbol);
    SymbolValue symbolValue;
    SymbolValue &sv = (it != table.end()) ? it->second : symbolValue;
    sv.hasValue = true;
    sv.value = value;
    table[symbol] = sv;
}

SymbolValue *DebugSymbolTable::lookup(std::string &symbol, llvm::Value *value) {
    auto val = table.find(symbol) == table.end() ? nullptr : &table[symbol];
    if (scope == value || !scope || !value || !child)
        return val;
    auto valChild = child->lookup(symbol, value);
    return valChild ? valChild : val;
}

void DebugSymbolTable::destroy() {
    if (child) {
        child->destroy();
        delete child;
        child = 0;
    }
}

}