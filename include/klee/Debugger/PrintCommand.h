#ifndef KLEE_PRINTCOMMAND_H
#define KLEE_PRINTCOMMAND_H

#include <string>

#include "klee/Debugger/DebugCommand.h"
#include "../../../lib/Core/Executor.h"
#include "../../../lib/Core/Searcher.h"

namespace klee {

class PrintCommand : public DebugCommandObject {
public:
    PrintCommand(Executor *executor, DebugSearcher *searcher);
    virtual std::string getName() { return "print"; }
    virtual void execute(CommandResult &res);

private:
    std::string var;
    Executor *executor;
    DebugSearcher *searcher;
};

class PrintRegisterCommand : public DebugCommandObject {
public:
    PrintRegisterCommand(DebugSearcher *searcher);
    virtual std::string getName() { return "print register"; }
    virtual void execute(CommandResult &res);

private:
    int registerNumber;
    DebugSearcher *searcher;
};

class ListCodeCommand : public DebugCommandObject {
public:
    ListCodeCommand(DebugSearcher *searcher, InstructionInfoTable *infos);
    virtual std::string getName() { return "list"; }
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res);
    virtual void execute(CommandResult &res);

private:
    std::string location;
    DebugSearcher *searcher;
    InstructionInfoTable *infos;
};


};

#endif