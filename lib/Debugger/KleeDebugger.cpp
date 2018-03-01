#include <assert.h>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <vector>

// FIXME: Probably not a good idea?
#include "../Core/Searcher.h"
#include "../Core/StatsTracker.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/Debugger/clipp.h"
#include "klee/Debugger/KleeDebugger.h"
#include "klee/Debugger/Prompt.h"
#include "klee/Debugger/linenoise.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Statistics.h"

namespace klee {

namespace {

using namespace clipp;
CommandType selected = CommandType::none;
InfoOpt infoOpt = InfoOpt::all;
StateOpt stateOpt = StateOpt::invalid;
std::string bpString;
int stateIdx = 0;
bool terminateOtherStates = false;

auto contcmd = (command("c").set(selected, CommandType::cont));
auto runcmd = (command("r").set(selected, CommandType::run));
auto stepcmd = (command("s").set(selected, CommandType::step));
auto quitcmd = (command("q").set(selected, CommandType::quit));
auto help = (command("h").set(selected, CommandType::help));
auto bcmd = (command("b").set(selected, CommandType::breakpoint), 
             values("", bpString));
auto info = (command("info").set(selected, CommandType::info),
             one_of(option("breakpoints").set(infoOpt, InfoOpt::breakpoints),
                    option("stack").set(infoOpt, InfoOpt::stack),
                    option("constraints").set(infoOpt, InfoOpt::constraints),
                    option("stats").set(infoOpt, InfoOpt::statistics),
                    option("all").set(infoOpt, InfoOpt::all)));

auto changestate = (command("state").set(selected, CommandType::state),
             one_of(command("next").set(stateOpt, StateOpt::next),
                    command("prev").set(stateOpt, StateOpt::prev)));

auto cli = one_of(contcmd , runcmd , stepcmd, quitcmd , bcmd , help , info, changestate);

auto select_branch = (
    value("State Number", stateIdx),
    option("-t", "--terminate", "-T").set(terminateOtherStates)
);


std::string getSourceLine(const std::string &file, unsigned line) {
    std::string res("");
    std::ifstream in;
    in.open(file.c_str());
    if (!in.fail()) {
        for (unsigned i = 1; i <= line; ++i) {
            std::getline(in, res);
            if (in.eof()) { 
                res = "";
                break;
            }
        }
        in.close();
    }
    return res;
}

std::string getFileFromPath(const std::string &fullPath) {
    std::cmatch matches;
    std::regex fileRegex(".*\\/(\\w*\\.\\w*)");
    if (std::regex_search(fullPath.c_str(), matches, fileRegex)) {
        return matches[1].str();
    }
    return "Unkown file";
}


}

extern "C" void set_halt_execution(bool);
extern "C" void set_interrupted(bool);
extern "C" bool klee_interrupted();

KDebugger::KDebugger() : 
    prompt(this),
    searcher(0),
    statsTracker(0),
    m_breakpoints(), 
    step(false) {}

void KDebugger::selectState() {
    if (klee_interrupted()) {
        step = false;
        set_interrupted(false);
        set_halt_execution(false);
        prompt.show("KLEE: ctrl-c detected, execution interrupted> ");
    } else if (step) {
        // Check if execution branched.
        if (searcher->newStates()) {
            llvm::outs().changeColor(llvm::raw_ostream::GREEN);
            llvm::outs() << "Execution branched\n";
            auto it = searcher->newStatesBegin();
            it--;
            for (int i = 0; i <= searcher->newStates(); ++i) {
                llvm::outs().changeColor(llvm::raw_ostream::CYAN);
                llvm::outs() << "State #" << (i + 1) << ", ";
                llvm::outs().changeColor(llvm::raw_ostream::WHITE);
                printConstraints(*it++);
                llvm::outs() << "\n";
            }
            if (prompt.show("Select a state to continue by entering state number:")) {
                return;
            }
        }
        step = false;
        showPromptAtInstruction(searcher->currentState()->pc);
    } else {
        checkBreakpoint(*searcher->currentState());
    }
}

void KDebugger::showPrompt(const char *msg) {
    assert(msg);
    prompt.show(msg);
}

void KDebugger::showPromptAtInstruction(const KInstruction *ki) {
    std::string file = getFileFromPath(ki->info->file);
    std::string msg = file + ", line " + std::to_string(ki->info->line) + "> ";
    prompt.show(msg.c_str());
}

void KDebugger::checkBreakpoint(ExecutionState &state) {
    if (m_breakpoints.empty() || !state.pc) {
        return;
    }
    auto ki = state.pc;
    Breakpoint bp(getFileFromPath(ki->info->file), ki->info->line);
    if (state.lastBreakpoint != bp && m_breakpoints.find(bp) != m_breakpoints.end()) {
        state.lastBreakpoint = bp;
        showPromptAtInstruction(ki);
    }
}


const std::set<Breakpoint> & KDebugger::breakpoints() {
    return m_breakpoints;
}

void KDebugger::setSearcher(DebugSearcher *searcher) {
    this->searcher = searcher;
    this->searcher->setPreSelectCallback(
        std::bind(&KDebugger::selectState, this));
}

void KDebugger::handleCommand(std::vector<std::string> &input, std::string &msg) {
    if (step && searcher->newStates()) {
        auto res = clipp::parse(input, select_branch);
        if (res) {
            selectBranch(stateIdx, msg);
        }
        return;
    }
    auto res = clipp::parse(input, cli);
    if (res) {
        switch(selected) {
            case CommandType::cont:
                handleContinue();
                break;
            case CommandType::run:
                handleRun();
                break;
            case CommandType::step:
                handleStep();
                break;
            case CommandType::quit:
                handleQuit();
                break;
            case CommandType::breakpoint:
                handleBreakpoint(bpString);
                break;
            case CommandType::info:
                handleInfo(infoOpt);
                break;
            case CommandType::state:
                handleState(stateOpt, msg);
                break;
            case CommandType::help:
                handleHelp();
                break;
            default:
                llvm::outs() << "invalid\n";
        }
    }  else {
        if (res.begin()->blocked()) {
            llvm::outs() << "Unkown command: " << res.begin()->arg() << "\n";
        } else {
            llvm::outs() << res.begin()->arg() << ": ";
            for(const auto& m : res) {
                if (m.blocked() && m.conflict()) {
                    llvm::outs() << "invalid argument(s)\n";
                    return;
                } else if (m.blocked()) {
                    llvm::outs() << "too many arguments\n";
                    return;
                }
            } 
            llvm::outs() << "argument required\n";
        }
    }
}

void KDebugger::handleContinue() {
    llvm::outs() << "Continue execution..\n";
    prompt.breakFromLoop();
}

void KDebugger::handleRun() {
    llvm::outs() << "Continue execution to end of program..\n";
    m_breakpoints.clear();
    prompt.breakFromLoop();
}

void KDebugger::handleStep() {
    llvm::outs() << "Stepping..\n";
    step = true;
    auto state = searcher->currentState();
    auto info = state->pc->info;
    std::string line = getSourceLine(info->file, info->line);
    size_t first = line.find_first_not_of(' ');
    std::string trimmed = line.substr(first, line.size() - first + 1);
    llvm::outs() << "source: " << trimmed << "\n";
    state->pc->printFileLine();
    llvm::outs() << "assembly line:";
    state->pc->inst->print(llvm::outs());
    llvm::outs() << "\n";
    prompt.breakFromLoop();
}

void KDebugger::handleQuit() {
    set_halt_execution(true);
    prompt.breakFromLoop();
}

void KDebugger::handleBreakpoint(std::string &input) {
    std::regex breakpointRegex("(\\w*\\.\\w*)\\:([0-9]+)");
    std::cmatch matches;
    if (std::regex_search(input.c_str(), matches, breakpointRegex)) {
        auto res = m_breakpoints.emplace(matches.str(1), (unsigned)stoi(matches.str(2)));
        klee_message(res.second ? "Breakpoint set" : "Breakpoint already exist");
    } else {
        klee_message("Invalid breakpoint format");
    }
}

void KDebugger::handleInfo(InfoOpt opt) {
    auto state = searcher->currentState();
    switch (opt) {
        case InfoOpt::stack: printStack(state); break;
        case InfoOpt::constraints: printConstraints(state); break;
        case InfoOpt::breakpoints: printBreakpoints(); break;
        case InfoOpt::statistics: this->statsTracker->printStats(llvm::outs()); break;
        case InfoOpt::all:
            printStack(state);
            llvm::outs() << "\n";
            printConstraints(state);
            break;
        default:
            llvm::outs() << "Invalid info option, type \"h\" for help.\n";
    }
}

void KDebugger::handleState(StateOpt dir, std::string &msg) {
    searcher->nextIter();
    auto *state = searcher->currentState();
    llvm::outs() <<  "Moved to state @"; 
    llvm::outs().write_hex((unsigned long long)state);
    llvm::outs() << "\n";
    msg = getFileFromPath(state->pc->info->file) + ", line " + 
          std::to_string(state->pc->info->line) + "> ";
}

void KDebugger::selectBranch(int idx, std::string &msg) {
    unsigned int newStates = searcher->newStates();
    if (idx < 1 || idx > (1 + newStates)) {
        return;
    }

    searcher->selectNewState(idx, terminateOtherStates);
    llvm::outs() << "You selected state #" << idx << ".\n";
    if (terminateOtherStates) {
        llvm::outs() << "All other states are terminated.\n";
    }
    prompt.breakFromLoop();
}

void KDebugger::printStats() {}

void KDebugger::handleHelp() {
    klee_message("\n  Type r to run the program.\n"
                 "  Type q to quit klee.\n"
                 "  Type c to continue until the next breakpoint.\n"
                 "  Type b <filename>:<linenumber> to add a breakpoint.\n"
                 "  Type info [stack | constraints | breakpoints | stats] to show information about the current execution state.\n"
                 "  Type state [prev | next] to traverse available states.\n");
}

void KDebugger::printBreakpoints() {
    klee_message("%zu breakpoints set", m_breakpoints.size());
    for (auto & bp : m_breakpoints) {
        klee_message("%s at line %u", bp.file.c_str(), bp.line);
    }
}

void KDebugger::printStack(ExecutionState *state) {
    // auto state = searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "Stack dump:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    state->dumpStack(llvm::outs());
}

void KDebugger::printConstraints(ExecutionState *state) {
    // auto state = searcher->currentState();
    llvm::outs().changeColor(llvm::raw_ostream::CYAN);
    llvm::outs() << "Constraints for state:\n";
    llvm::outs().changeColor(llvm::raw_ostream::WHITE);
    ExprPPrinter::printConstraints(llvm::outs(), state->constraints);
}

}