#include "Command_Proc.h"
#include <sstream>
#include <algorithm>
#include <cctype>

CommandProcessor::CommandProcessor() {
    RegisterStringCommands(*this);
    RegisterListCommands(*this);
    RegisterSetCommands(*this);
    RegisterGeoCommands(*this);
    RegisterGenCommands(*this);
}

void CommandProcessor::register_command(const std::string& name, std::unique_ptr<Command> cmd) {
    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), 
                   [](unsigned char c){ return std::toupper(c); });
                   
    commands_[upper_name] = std::move(cmd);
}

std::vector<std::string> CommandProcessor::tokenize(const std::string& line) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false; 
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                in_quotes = true;
                quote_char = c; 
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string CommandProcessor::process(Engine& engine, const std::string& line) {
    auto args = tokenize(line);
    if (args.empty()) {
        return ""; 
    }

    std::string cmd_name = args[0];
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), 
                   [](unsigned char c){ return std::toupper(c); });

    auto it = commands_.find(cmd_name);
    if (it == commands_.end()) return "(error) ERR unknown command '" + args[0] + "'\n";

    try {
        return it->second->execute(engine, args);
    } 
    catch (const OOMError& e) {
        return std::string(e.what()) + "\n";
    } 
    catch (const std::exception& e) {
        return "(error) ERR " + std::string(e.what()) + "\n";
    }
}