#pragma once

#include <regex>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Engine.h"

class WrongTypeError : public std::runtime_error {
public:
    WrongTypeError() : std::runtime_error("WRONGTYPE Operation against a key holding the wrong kind of value") {}
};

template <typename ExpectedType>
inline void require_type(const Value* val) {
    if (val && !std::holds_alternative<ExpectedType>(val->data)) {
        throw WrongTypeError();
    }
}

inline size_t parse_memory_string(const std::string& mem_str) {
    std::regex re("^(\\d+)(b|kb|mb|gb)?$", std::regex_constants::icase);
    std::smatch match;
    if (std::regex_match(mem_str, match, re)) {
        size_t val = std::stoull(match[1]);
        std::string suffix = match[2];
        for (auto& c : suffix) c = std::tolower(c);
        
        if (suffix == "kb") val *= 1024;
        else if (suffix == "mb") val *= 1024 * 1024;
        else if (suffix == "gb") val *= 1024 * 1024 * 1024;
        return val;
    }
    throw std::invalid_argument("invalid memory size format");
}

class Command {
public:
    virtual ~Command() = default;
    virtual std::string execute(Engine& engine, const std::vector<std::string>& args) = 0;
};


class CommandProcessor {
public:
    CommandProcessor();
    ~CommandProcessor() = default;


    CommandProcessor(const CommandProcessor&) = delete;
    CommandProcessor& operator=(const CommandProcessor&) = delete;

    std::string process(Engine& engine, const std::string& line);
    void register_command(const std::string& name, std::unique_ptr<Command> cmd);

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
    std::vector<std::string> tokenize(const std::string& line) const;
};

void RegisterStringCommands(CommandProcessor& processor);
void RegisterListCommands(CommandProcessor& processor);
void RegisterSetCommands(CommandProcessor& processor);
void RegisterGeoCommands(CommandProcessor& processor);
void RegisterGenCommands(CommandProcessor& processor);