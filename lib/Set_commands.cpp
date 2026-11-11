#include "Command_Proc.h"
#include <unordered_set>
#include <string>
#include <vector>

inline std::string format_set(const SetType& set) {
    if (set.empty()) {
        return "(empty array)\n";
    }
    std::string result;
    int idx = 1;
    for (const auto& member : set) {
        result += std::to_string(idx++) + ") \"" + member + "\"\n";
    }
    return result;
}

class CmdSAdd : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'sadd' command");
        
        Value* val = engine.get(args[1]);
        require_type<SetType>(val);

        SetType set = val ? std::get<SetType>(val->data) : SetType{};
        time_t ttl = val ? val->expire_at : 0;

        int added = 0;
        for (size_t i = 2; i < args.size(); ++i) {
            if (set.insert(args[i]).second) {
                added++;
            }
        }
        
        engine.set(args[1], Value(set, ttl));
        return "(integer) " + std::to_string(added) + "\n";
    }
};

class CmdSRem : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'srem' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n"; 
        require_type<SetType>(val);

        SetType set = std::get<SetType>(val->data);
        int removed = 0;
        for (size_t i = 2; i < args.size(); ++i) {
            if (set.erase(args[i])) {
                removed++;
            }
        }

        if (set.empty()) {
            engine.del(args[1]); 
        } else {
            engine.set(args[1], Value(set, val->expire_at));
        }

        return "(integer) " + std::to_string(removed) + "\n";
    }
};

class CmdSIsMember : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'sismember' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n";
        require_type<SetType>(val);

        const SetType& set = std::get<SetType>(val->data);
        if (set.find(args[2]) != set.end()) {
            return "(integer) 1\n";
        }
        return "(integer) 0\n";
    }
};

class CmdSMembers : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'smembers' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(empty array)\n";
        require_type<SetType>(val);

        const SetType& set = std::get<SetType>(val->data);
        return format_set(set);
    }
};

class CmdSCard : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'scard' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n";
        require_type<SetType>(val);

        const SetType& set = std::get<SetType>(val->data);
        return "(integer) " + std::to_string(set.size()) + "\n";
    }
};

class CmdSUnion : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 2) throw std::invalid_argument("wrong number of arguments for 'sunion' command");
        
        SetType union_set;
        for (size_t i = 1; i < args.size(); ++i) {
            Value* val = engine.get(args[i]);
            if (!val) continue; 
            require_type<SetType>(val);
            
            const SetType& current_set = std::get<SetType>(val->data);
            union_set.insert(current_set.begin(), current_set.end());
        }

        return format_set(union_set);
    }
};


class CmdSInter : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 2) throw std::invalid_argument("wrong number of arguments for 'sinter' command");
        
        Value* val1 = engine.get(args[1]);
        if (!val1) return "(empty array)\n";
        require_type<SetType>(val1);
        const SetType& first_set = std::get<SetType>(val1->data);

        std::vector<const SetType*> subsequent_sets;
        for (size_t i = 2; i < args.size(); ++i) {
            Value* next_val = engine.get(args[i]);
            if (!next_val) return "(empty array)\n";
            require_type<SetType>(next_val);
            subsequent_sets.push_back(&std::get<SetType>(next_val->data));
        }

        SetType intersection;
        for (const auto& item : first_set) {
            bool in_all = true;
            for (const auto* s : subsequent_sets) {
                if (s->find(item) == s->end()) {
                    in_all = false;
                    break;
                }
            }
            if (in_all) {
                intersection.insert(item);
            }
        }

        return format_set(intersection);
    }
};

class CmdSDiff : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 2) throw std::invalid_argument("wrong number of arguments for 'sdiff' command");
        
        Value* val1 = engine.get(args[1]);
        if (!val1) return "(empty array)\n";
        require_type<SetType>(val1);
        
        SetType diff_set = std::get<SetType>(val1->data);

        for (size_t i = 2; i < args.size(); ++i) {
            Value* next_val = engine.get(args[i]);
            if (!next_val) continue;
            require_type<SetType>(next_val);
            
            const SetType& next_set = std::get<SetType>(next_val->data);
            for (const auto& item : next_set) {
                diff_set.erase(item);
            }
        }

        return format_set(diff_set);
    }
};

class CmdSMove : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 4) throw std::invalid_argument("wrong number of arguments for 'smove' command");
        
        std::string source_key = args[1];
        std::string dest_key = args[2];
        std::string member = args[3];

        Value* src_val = engine.get(source_key);
        if (!src_val) return "(integer) 0\n";
        require_type<SetType>(src_val);
        SetType src_set = std::get<SetType>(src_val->data);

        auto it = src_set.find(member);
        if (it == src_set.end()) {
            return "(integer) 0\n";
        }

        Value* dest_val = engine.get(dest_key);
        require_type<SetType>(dest_val);
        SetType dest_set = dest_val ? std::get<SetType>(dest_val->data) : SetType{};
        time_t dest_ttl = dest_val ? dest_val->expire_at : 0;

        src_set.erase(it);
        if (src_set.empty()) {
            engine.del(source_key);
        } else {
            engine.set(source_key, Value(src_set, src_val->expire_at));
        }

        dest_set.insert(member);
        engine.set(dest_key, Value(dest_set, dest_ttl));

        return "(integer) 1\n";
    }
};


void RegisterSetCommands(CommandProcessor& processor) {
    processor.register_command("SADD", std::make_unique<CmdSAdd>());
    processor.register_command("SREM", std::make_unique<CmdSRem>());
    processor.register_command("SISMEMBER", std::make_unique<CmdSIsMember>());
    processor.register_command("SMEMBERS", std::make_unique<CmdSMembers>());
    processor.register_command("SCARD", std::make_unique<CmdSCard>());
    processor.register_command("SUNION", std::make_unique<CmdSUnion>());
    processor.register_command("SINTER", std::make_unique<CmdSInter>());
    processor.register_command("SDIFF", std::make_unique<CmdSDiff>());
    processor.register_command("SMOVE", std::make_unique<CmdSMove>());
}