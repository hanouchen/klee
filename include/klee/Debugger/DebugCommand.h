#ifndef KLEE_DEBUGCOMMAND_H
#define KLEE_DEBUGCOMMAND_H

#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "klee/Debugger/clipp.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"

namespace klee {

class DebugCommand;
struct CommandResult {
    bool success;
    bool stayInDebugger;
    std::string msg;
    std::string usageString;
    void setMsg(std::string message) {
        msg = message;
    }
};

struct ParsingResult {
    bool success = false;
    std::string errMsg = {};
    DebugCommand *command = nullptr;
    void setErr(std::string message) {
        errMsg = message;
    }
};


class DebugCommand {
public:
    virtual ~DebugCommand() { destroy(); }
    virtual void execute(CommandResult &res) = 0;
    virtual std::string getName() = 0;
    virtual std::string usageString() = 0;
    virtual std::string docString() = 0;
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res) = 0;
    virtual void destroy() {}
};




class DebugCommandClipp : public DebugCommand {
public:
    virtual void execute(CommandResult &res) = 0;
    virtual std::string getName() = 0;
    virtual std::string usageString() {
        return clipp::usage_lines(parser).str() + "\n";
    }
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res) = 0;

protected:
    std::vector<std::string> extraArgs;
    clipp::group command;
    clipp::group parser;
};

class DebugCommandObject : public DebugCommandClipp {
public:
    virtual void execute(CommandResult &res) = 0;
    virtual std::string getName() = 0;
    virtual std::string docString() {
        std::string doc = {};
        const unsigned usageCol = 0;
        const unsigned docCol = 28;
        llvm::raw_string_ostream ss(doc);
        llvm::formatted_raw_ostream fo(ss);
        fo.PadToColumn(usageCol);
        auto fmt = clipp::doc_formatting{}.start_column(0);
        fo << clipp::usage_lines(command, fmt);
        fo.PadToColumn(docCol);
        // fo << parser.doc();
        fo << command.doc();
        fo.flush();
        ss.flush();
        doc = ss.str();
        return doc;
    }

    virtual std::string usageString() {
        auto fmt = clipp::doc_formatting{}.start_column(1);
        std::string msg = {};
        llvm::raw_string_ostream ss(msg);
        ss << clipp::make_man_page(parser, "", fmt) << "\n";
        ss.flush();
        return ss.str();
    }

    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res) {
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
            } else if (!fullMatch && extraArgs.empty()) {
                res.setErr(std::string("Missing argument(s) for \"") + getName() + "\"\n");
            } else {
                std::string errMsg = "Unsupported option or argument";
                errMsg += " for \"" + getName() + "\": " + optsAndArgs[0] + "\n";
                res.setErr(errMsg);
            }
        } else {
            res.setErr("Unknown command\n");
        }
    }

};

class DebugCommandList : public DebugCommandClipp {
public:
    virtual void execute(CommandResult &res) {}
    virtual std::string getName() { return ""; }
    virtual void parse(std::vector<std::string> &tokens, ParsingResult &res) {
        res.success = false;
        res.command = nullptr;
        extraArgs.clear();
        clipp::parsing_result prefixMatch;
        if (getName() != "") {
            prefixMatch = clipp::parse(tokens, command);
            if (!prefixMatch) {
                return;
            }
        }
        for (auto cmd : subCommands) {
            cmd->parse(tokens, res);
            if (res.success || res.command) {
                return;
            }
        }
        if (getName() == "") {
            res.setErr("Unkown command\n");
            return;
        }
        if (prefixMatch) {
            res.command = this;
        }
        if (extraArgs.empty()) {
            res.setErr("No valid command provided\n");
        } else {
            std::string errMsg = "Unsupported option or argument";
            errMsg += " for \"" + getName() + "\": " + extraArgs[0] + "\n";
            res.setErr(errMsg);
        }
    }
    virtual std::string docString() {
        if (getName() == "") return usageString() + "\n"; // root command
        auto fmt = clipp::doc_formatting{}.start_column(1).doc_column(28);
        return clipp::documentation(command, fmt).str();
    }

    virtual std::string usageString() {
        return (getName() == "" ? "" : "USAGE:\n") + usage + "\n";
    }

    virtual void destroy() {
        for (auto *cmd : subCommands) {
            cmd->destroy();
            delete cmd;
            cmd = nullptr;
        }
    }

    void addSubCommand(DebugCommand *subCommand) {
        subCommands.push_back(subCommand);
        usage += subCommand->docString() + "\n";
    }

protected:
    std::string usage = {};
    std::vector<DebugCommand *> subCommands;
};

};
#endif