#include <string>

#include "klee/Debugger/clipp.h"
#include "klee/Debugger/DebugUtil.h"
#include "klee/Debugger/DebugSymbolTable.h"
#include "klee/Debugger/PrintCommand.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "../lib/Core/Executor.h"
#include "../lib/Core/Searcher.h"
#include "../lib/Core/Memory.h"
#include "llvm/DebugInfo.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Metadata.h"

namespace klee {

PrintCommand::PrintCommand(Executor *executor, DebugSearcher *searcher) :
        executor(executor), searcher(searcher) {
    command = clipp::group(clipp::command("print", "p"));
    parser = (
        clipp::group(command),
        clipp::value("variable").set(var),
        clipp::any_other(extraArgs)
    );
    command.doc("Print the (symbolic or concrete) value of a variable");
    command.push_back(clipp::any_other(extraArgs));
}

void PrintCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    res.setMsg("");
    llvm::outs() << "Printing variable: " << var << "\n";
    llvm::MDNode *md = searcher->currentState()->prevPC->inst->getMetadata("dbg");
    llvm::Value *scope = nullptr;
    if (md) {
        scope = md->getOperand(2);
    }
    auto &stack = searcher->currentState()->stack;
    auto &sf = stack.back();
    auto svalue = sf.st.lookup(var, scope);
    if (!svalue) {
        llvm::outs() << "Unable to print variable information\n";
        return;
    }
    if (svalue != nullptr && svalue->hasAddress) {
        auto state = searcher->currentState();
        Expr::Width type = executor->getWidthForLLVMType(svalue->type);
        auto addr = svalue->address;
        if (!isa<ConstantExpr>(addr))
            addr = state->constraints.simplifyExpr(addr);
        ObjectPair op;
        bool success;
        state->addressSpace.resolveOne(*state, executor->solver, addr, op, success);
        if (success) {
            auto mo = op.first;
            auto os = op.second;
            llvm::outs() << os->read(mo->getOffsetExpr(addr), type) << "\n";
            return;
        }
    } else {
        auto state = searcher->currentState();
        int regIdx = -1;
        llvm::Value *value = nullptr;
        if (svalue && svalue->hasValue) value = svalue->value;
        for (unsigned i = 0; i < sf.kf->numInstructions; ++i) {
            if (state->pc == sf.kf->instructions[i]) {
                break;
            }
            std::string temp = sf.kf->instructions[i]->inst->getName().str();
            auto res = std::mismatch(var.begin(), var.end(), temp.begin());
            if (sf.kf->instructions[i]->inst == value || res.first == var.end()) {
                regIdx = sf.kf->instructions[i]->dest;
            }
        }
        if (regIdx != -1) {
            llvm::outs() << sf.locals[regIdx].value << "\n";
        } else if (value) {
			llvm::outs() << "Use value\n";
            value->dump();
        } else {
            llvm::outs() << "Unable to print variable information\n";
        }
    }

}

ListCodeCommand::ListCodeCommand(DebugSearcher *searcher, InstructionInfoTable *infos) :
        location(), searcher(searcher), infos(infos) {
    command = clipp::group(clipp::command("list"));
    parser = (
        clipp::group(command),
        clipp::opt_value("location").set(location).doc("Location in the current source file, either <line> or <function>"),
        clipp::any_other(extraArgs)
    );
    command.doc("Print the source code at the given location");
    command.push_back(clipp::any_other(extraArgs));
}

void ListCodeCommand::parse(std::vector<std::string> &tokens, ParsingResult &res) {
    res.success = false;
    res.command = nullptr;
    extraArgs.clear();
    auto partialMatch = clipp::parse(tokens, command);
    std::vector<std::string> optsAndArgs(extraArgs);
    if (partialMatch) {
        res.command = this;
        extraArgs.clear();
        auto fullMatch = clipp::parse(tokens, parser);
        if (fullMatch && extraArgs.empty()) {
            res.success = true;
            return;
        }
        if (fullMatch && !extraArgs.empty()) {
            std::string errMsg = "Unsupported option or argument";
            errMsg += " for \"" + getName() + "\": " + extraArgs[0] + "\n";
            res.setErr(errMsg);
        } else if (!fullMatch && tokens.size() > 2) {
            std::string errMsg = "Unsupported option or argument";
            errMsg += " for \"" + getName() + "\": " + tokens[2] + "\n";
            res.setErr(errMsg);
        }
    } else {
        res.setErr("Unknown command\n");
    }
}

void ListCodeCommand::execute(CommandResult &res) {
    res.stayInDebugger = true;
    bool lineProvided = false;
    auto state = searcher->currentState();
    int lineNumber = 0;
    std::string file = state->pc->info->file;
    if (location == "") {
        lineNumber = state->pc->info->line;
    } else {
        std::stringstream ss(location);
        if (ss >> lineNumber) {
            lineProvided = true;
        } else {
            lineNumber = 0;
        }
    }

    if (lineNumber == 0) {
        auto info = infos->getFunctionInfoByName(location);
        lineNumber = info.line;
        file = info.file;
    }

    int from = std::max(1, lineNumber - 4);
    int to = lineNumber + (9 - (lineNumber - from));

    std::string code = debugutil::getSourceLine(file, from, to);
    if (code != "") {
        llvm::outs() << code;
        location = {};
        return;
    }
    if (lineProvided) {
        llvm::outs() << "Line " << lineNumber << " ";
    } else {
        llvm::outs() << "Function " << location << " ";
    }
    llvm::outs() << "out of range.\n";
    location = {};
}



};