#include <fstream>
#include <string>

#include "klee/Debugger/DebugUtil.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/util/ExprPPrinter.h"


namespace klee {

namespace debugutil {

std::string getSourceLine(const std::string &file, int from, int to) {
    std::string codeBlock = {};
    std::string line = {};
    std::ifstream in;
    if (to == -1) to = from;
    in.open(file.c_str());
    if (!in.fail()) {
        for (int i = 1; i <= to; ++i) {
            std::getline(in, line);
            if (i >= from) {
                codeBlock += std::to_string(i) + "  " + line;
                if (from < to) codeBlock += '\n';
            }
            if (in.eof()) {
                line = "";
                break;
            }
        }
        in.close();
    }
    return codeBlock;
}

std::string getFileFromPath(const std::string &fullPath) {
    size_t i = fullPath.find_last_of('/');
    if (i == std::string::npos) {
        return fullPath;
    }
    return fullPath.substr(i + 1);
}

void printState(ExecutionState *state, PrintStateOption option, llvm::raw_ostream &os) {
    assert(state);
    os.changeColor(llvm::raw_ostream::CYAN);
    os << "state @" << state << ", ";
    if (option == PrintStateOption::COMPACT) {
        os << "\n";
    }
    if (option == PrintStateOption::DEFAULT)
        printConstraints(state, os);
    if (option == PrintStateOption::DEFAULT) {
        printCode(state->pc);
        os << "\n";
    } else {
        printCode(state->pc, PrintCodeOption::SOURCE_ONLY, os);
    }
}

void printCode(KInstIterator ki, PrintCodeOption option, llvm::raw_ostream &os) {
    assert(ki);
    os.changeColor(llvm::raw_ostream::WHITE);

    auto file = ki->info->file;
    auto line = ki->info->line;

    if (file == "") {
        os  << "No corresponding source information\n";
    } else {
        std::string source = getSourceLine(file, line);
		if (source == "") {
			os  << "No corresponding source information\n";
		} else {
			if (option != PrintCodeOption::SOURCE_ONLY)
                os << "loc: " << file << ":" << line << "\n";
			os << "source: " << source;
		}
    }
    if (option == PrintCodeOption::DEFAULT) {
        os << "\nassembly:";
        ki->inst->print(os);
    } else {
        os << "\n";
    }
}

void printStack(ExecutionState *state, llvm::raw_ostream &os) {
    assert(state);
    os.changeColor(llvm::raw_ostream::CYAN);
    os << "stack dump:\n";
    os.changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(os);
}

void printConstraints(ExecutionState *state, llvm::raw_ostream &os) {
    assert(state);
    os.changeColor(llvm::raw_ostream::CYAN);
    os << "constraints for state:\n";
    os.changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(os, state->constraints);
}

};

};