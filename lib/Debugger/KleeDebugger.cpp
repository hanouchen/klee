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
    const std::string LIST = "l";
    const std::string HELP = "h";
    const char BREAK = 'b';
    const char * DEFAULT_PROMPT = "klee debugger, type h for help> ";
}


void KDebugger::showPrompt() {
    char *line;
    while((line = linenoise(DEFAULT_PROMPT)) != NULL) {
        if (line == QUIT) {
            m_quitKlee = true;
            break;
        } else if (line == RUN) {
            break;
        } else if (line[0] == BREAK) {
            addBreakpointFromLine(line + 1);
        } else if (line == LIST) {
            printBreakpoints();
        } else if (line == HELP) {
            printHelp();
        } else if (line[0] != '\0'){
            printf("Invalid command, type h to see list of commands"); 
            linenoiseHistoryAdd(line);
        }

        linenoiseFree(line); 
    }
    return;
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
        showPrompt();
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
    klee_message("\n\tType r to continue execution.\n"
                 "\tType q to quit klee.\n"
                 "\tType b <filename>:<linenumber> to add a breakpoint\n"
                 "\tType l to list all the breakpoints set");
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