#include "Command_Proc.h"
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <stdexcept>

class CmdType : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'type' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "none\n";
        
        return val->get_type_name() + "\n";
    }
};

class CmdDel : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 2) throw std::invalid_argument("wrong number of arguments for 'del' command");
        
        int deleted_count = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (engine.del(args[i])) {
                deleted_count++;
            }
        }
        return "(integer) " + std::to_string(deleted_count) + "\n";
    }
};

class CmdExists : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 2) throw std::invalid_argument("wrong number of arguments for 'exists' command");
        
        int count = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (engine.exists(args[i])) {
                count++;
            }
        }
        return "(integer) " + std::to_string(count) + "\n";
    }
};

class CmdKeys : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'keys' command");
        
        auto matched_keys = engine.keys(args[1]);
        if (matched_keys.empty()) {
            return "(empty array)\n";
        }

        std::string result;
        int idx = 1;
        for (const auto& key : matched_keys) {
            result += std::to_string(idx++) + ") \"" + key + "\"\n";
        }
        return result;
    }
};

class CmdFlushDB : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 1) throw std::invalid_argument("wrong number of arguments for 'flushdb' command");
        
        engine.flushdb();
        return "OK\n";
    }
};

class CmdDbSize : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 1) throw std::invalid_argument("wrong number of arguments for 'dbsize' command");
        
        return "(integer) " + std::to_string(engine.dbsize()) + "\n";
    }
};

class CmdConfig : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'config' command");
        
        std::string subcmd = args[1];
        std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);

        if (subcmd == "GET") {
            if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'config get' command");
            
            std::string param = args[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            
            if (param == "maxmemory") {
                return "1) \"maxmemory\"\n2) \"" + std::to_string(engine.get_max_memory()) + "\"\n";
            }
            throw std::invalid_argument("unsupported config parameter");
        } 
        else if (subcmd == "SET") {
            if (args.size() != 4) throw std::invalid_argument("wrong number of arguments for 'config set' command");
            
            std::string param = args[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            
            if (param == "maxmemory") {
                size_t bytes = parse_memory_string(args[3]);
                engine.set_max_memory(bytes);
                return "OK\n";
            }
            throw std::invalid_argument("unsupported config parameter");
        }

        throw std::invalid_argument("unsupported config subcommand");
    }
};

class CmdMemory : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'memory' command");
        
        std::string subcmd = args[1];
        std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);

        if (subcmd == "USAGE") {
            Value* val = engine.get(args[2]);
            if (!val) return "(nil)\n";
            
            size_t total_memory = 32 + args[2].capacity() + val->estimated_memory;
            return "(integer) " + std::to_string(total_memory) + "\n";
        }
        
        throw std::invalid_argument("unsupported memory subcommand");
    }
};


void RegisterGenCommands(CommandProcessor& processor) {
    processor.register_command("TYPE", std::make_unique<CmdType>());
    processor.register_command("DEL", std::make_unique<CmdDel>());
    processor.register_command("EXISTS", std::make_unique<CmdExists>());
    processor.register_command("KEYS", std::make_unique<CmdKeys>());
    processor.register_command("FLUSHDB", std::make_unique<CmdFlushDB>());
    processor.register_command("DBSIZE", std::make_unique<CmdDbSize>());
    processor.register_command("CONFIG", std::make_unique<CmdConfig>());
    processor.register_command("MEMORY", std::make_unique<CmdMemory>());
}