
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
DebugSymbolTable() : table() {}
DebugSymbolTable(const DebugSymbolTable &s) : table(s.table) {}

bool bindAddress(std::string &symbol, llvm::Value *address, StackFrame &sf);
void updateValue(std::string &symbol, llvm::Value *value);

SymbolValue *lookup(std::string &symbol);



private:
std::unordered_map<std::string, SymbolValue> table;

};

}

#endif