#include <assert.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

#include "llvm/Support/raw_ostream.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Internal/Support/ErrorHandling.h"


namespace klee {

namespace {
    const std::string QUIT = "q";
    const std::string RUN = "r";
    const std::string CONTINUE = "c";
    const std::string LIST = "l";
    const std::string HELP = "h";
    const std::string INFO = "info";
    const std::string INFO_STACK = INFO + " stack";
    const std::string INFO_CONSTRAINTS = INFO + " constraints";
    const char BREAK = 'b';
}

KDebugger::KDebugger() : m_prevBp("", 0), m_currentState(NULL), m_breakpoints() {}

void KDebugger::showPrompt(const char *prompt) {
    assert(prompt);

    char *line;
    while((line = linenoise(prompt)) != NULL) {
        if (line == QUIT) {
            break;
        } else if (line == CONTINUE) {
            break;
        } else if (line == RUN) {
            m_breakpoints.clear();
            break;
        } else if (line[0] == BREAK) {
            addBreakpointFromLine(line + 1);
        } else if (line == LIST) {
            printBreakpoints();
        } else if (line == HELP) {
            printHelp();
        } else if (strlen(line) >= INFO.length() && INFO == std::string(line).substr(0, INFO.length())) {
            printStateInfo(line);
        } else if (line[0] != '\0'){
            klee_message("Invalid command, type h to see list of commands"); 
        } 
        
        if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
        }
        linenoiseFree(line); 
    }
    return;
}

void KDebugger::showPromptAtBreakpoint(const Breakpoint &breakpoint) {
    std::string prompt = breakpoint.file + ", line " + std::to_string((int)breakpoint.line) + "> ";
    showPrompt(prompt.c_str());
}

void KDebugger::checkBreakpoint(const ExecutionState &state) {
    auto ki = state.pc;
    m_currentState = &state;
    if (!ki) {
        return;
    }
    std::regex fileRegex(".*\\/(\\w*\\.\\w*)");
    std::cmatch matches;
    if (std::regex_search(ki->info->file.c_str(), matches, fileRegex)) {
      Breakpoint bp(matches[1].str(), ki->info->line);
      if (m_prevBp != bp && m_breakpoints.find(bp) != m_breakpoints.end()) {
        m_prevBp = bp;
        showPromptAtBreakpoint(bp);
      }
    }
}

const std::set<Breakpoint> & KDebugger::breakpoints() {
    return m_breakpoints;
}


void KDebugger::printHelp() {
    klee_message("\n  Type r to run the program.\n"
                 "  Type q to quit klee.\n"
                 "  Type c to continue until the next breakpoint.\n"
                 "  Type b <filename>:<linenumber> to add a breakpoint.\n"
                 "  Type info [stack | constraints] to show information about the current execution state.\n"
                 "  Type l to list all the breakpoints set.");
}

void KDebugger::printBreakpoints() {
    klee_message("%zu breakpoints set", m_breakpoints.size());
    for (auto & bp : m_breakpoints) {
        klee_message("%s at line %u", bp.file.c_str(), bp.line);
    }
}

void KDebugger::addBreakpointFromLine(const char *line) {
    std::regex breakpointRegex("\\s+(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    if (std::regex_search(line, matches, breakpointRegex)) {
        auto res = m_breakpoints.emplace(matches[1].str(), (unsigned)stoi(matches[2].str()));
        klee_message(res.second ? "Breakpoint set" : "Breakpoint already exist");
    } else {
        klee_message("Invalid breakpoint format");
    }
}

void KDebugger::printStack() {
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    m_currentState->dumpStack(llvm::outs());
}

void KDebugger::printConstraints() {
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "KLEE:Constraints for current state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), m_currentState->constraints);
}

void KDebugger::printStateInfo(const char *line) {
    if (!m_currentState) {
        klee_message("Invalid execution state, no info to show.");
        return;
    }

    if (line == INFO) {
        printStack();
        llvm::outs() << "\n";
        printConstraints();
    } else if (line == INFO_STACK) {
        printStack();
    } else if (line == INFO_CONSTRAINTS) {
        printConstraints();
    } else {
        klee_message("Unkown info argument");
        return;
    }
}

}