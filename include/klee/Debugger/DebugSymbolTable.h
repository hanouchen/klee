
#ifndef KLEE_DEBUGSYMBOLTABLE_H
#define KLEE_DEBUGSYMBOLTABLE_H

#include <string>
#include <unordered_map>

#include <klee/Expr.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>

namespace klee {

class KFunction;
class Expr;
class StackFrame;

struct SymbolValue {
    SymbolValue() : hasAddress(false), hasValue(false), value(0) {}
    bool hasAddress;
    bool hasValue;
    ref<Expr> address;
    llvm::Value *value;
    llvm::Type *type;
};

class DebugSymbolTable {

public:
~DebugSymbolTable();
DebugSymbolTable() : table(), parent(0), child(0), scope(0) {}
DebugSymbolTable(const DebugSymbolTable &s) :
    table(s.table),
    parent(0),
    child(0),
    scope(s.scope) {

    if (s.parent) {
        parent = new DebugSymbolTable(*s.parent);
    }

    if (s.child) {
        child = new DebugSymbolTable(*s.child);
    }
}

bool bindAddress(std::string &symbol, llvm::Value *address, StackFrame &sf, llvm::Value *scope);
//void updateValue(std::string &symbol, llvm::Value *value);
void updateValue(std::string &symbol, llvm::Value *value, llvm::Value *scope);

SymbolValue *lookup(std::string &symbol, llvm::Value *scope);

void destroy();


private:
std::unordered_map<std::string, SymbolValue> table;
DebugSymbolTable *parent;
DebugSymbolTable *child;
llvm::Value *scope;

};

}

#endif
