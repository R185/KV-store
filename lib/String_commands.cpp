#include "Command_Proc.h"
#include <string>

class CmdSet : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'set' command");
        
        engine.set(args[1], Value(StringType(args[2])));
        return "OK\n";
    }
};

class CmdGet : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'get' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(nil)\n";
        
        require_type<StringType>(val); 
        
        return "\"" + std::get<StringType>(val->data) + "\"\n";
    }
};

class CmdStrlen : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'strlen' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n";
        
        require_type<StringType>(val);
        
        return "(integer) " + std::to_string(std::get<StringType>(val->data).size()) + "\n";
    }
};

class CmdAppend : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'append' command");
        
        Value* val = engine.get(args[1]);
        require_type<StringType>(val);
        
        StringType str = val ? std::get<StringType>(val->data) : "";
        time_t ttl = val ? val->expire_at : 0;
        
        str += args[2];
        size_t new_len = str.size();
        
        engine.set(args[1], Value(str, ttl));
        return "(integer) " + std::to_string(new_len) + "\n";
    }
};

class CmdExpire : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'expire' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n";
        
        long long seconds = std::stoll(args[2]);
        val->expire_at = get_current_time_sec() + seconds;
        engine.set(args[1], *val); 
        
        return "(integer) 1\n";
    }
};

class CmdTTL : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'ttl' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) -2\n";
        if (val->expire_at == 0) return "(integer) -1\n";
        
        long long ttl = val->expire_at - get_current_time_sec();
        return "(integer) " + std::to_string(ttl > 0 ? ttl : 0) + "\n";
    }
};

void RegisterStringCommands(CommandProcessor& processor) {
    processor.register_command("SET", std::make_unique<CmdSet>());
    processor.register_command("GET", std::make_unique<CmdGet>());
    processor.register_command("STRLEN", std::make_unique<CmdStrlen>());
    processor.register_command("APPEND", std::make_unique<CmdAppend>());
    processor.register_command("EXPIRE", std::make_unique<CmdExpire>());
    processor.register_command("TTL", std::make_unique<CmdTTL>());
}