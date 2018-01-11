#include <assert.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

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
    const char BREAK = 'b';
}


void KDebugger::showPrompt(const char *prompt) {
    assert(prompt);

    char *line;
    while((line = linenoise(prompt)) != NULL) {
        if (line == QUIT) {
            m_quitKlee = true;
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
        } else if (line[0] != '\0'){
            printf("Invalid command, type h to see list of commands"); 
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

void KDebugger::checkBreakpoint(const KInstruction *ki) {
    if (!ki) {
        return;
    }
    std::regex fileRegex(".*\\/(\\w*\\.\\w*)");
    std::cmatch matches;
    if (std::regex_search(ki->info->file.c_str(), matches, fileRegex)) {
      Breakpoint bp(matches[1].str(), ki->info->line);
      if (m_breakpoints.find(bp) != m_breakpoints.end()) {
        showPromptAtBreakpoint(bp);
      }
    }

}

const std::set<Breakpoint> & KDebugger::breakpoints() {
    return m_breakpoints;
}

bool KDebugger::quitKlee() {
    return m_quitKlee;
}

void KDebugger::printHelp() {
    klee_message("\n\tType r to run the program.\n"
                 "\tType q to quit klee.\n"
                 "\tType c to continue until the next breakpoint.\n"
                 "\tType b <filename>:<linenumber> to add a breakpoint.\n"
                 "\tType l to list all the breakpoints set.");
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

}