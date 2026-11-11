#include "Command_Proc.h"
#include <deque>
#include <algorithm>
#include <string>

class CmdLPush : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'lpush' command");
        
        Value* val = engine.get(args[1]);
        require_type<ListType>(val);

        ListType list = val ? std::get<ListType>(val->data) : ListType{};
        time_t ttl = val ? val->expire_at : 0;

        for (size_t i = 2; i < args.size(); ++i) {
            list.push_front(args[i]);
        }
        
        size_t size = list.size();
        engine.set(args[1], Value(list, ttl));
        return "(integer) " + std::to_string(size) + "\n";
    }
};

class CmdRPush : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'rpush' command");
        
        Value* val = engine.get(args[1]);
        require_type<ListType>(val);

        ListType list = val ? std::get<ListType>(val->data) : ListType{};
        time_t ttl = val ? val->expire_at : 0;

        for (size_t i = 2; i < args.size(); ++i) {
            list.push_back(args[i]);
        }
        
        size_t size = list.size();
        engine.set(args[1], Value(list, ttl));
        return "(integer) " + std::to_string(size) + "\n";
    }
};


class CmdLPop : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() > 3) throw std::invalid_argument("wrong number of arguments for 'lpop' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(nil)\n";
        require_type<ListType>(val);

        ListType list = std::get<ListType>(val->data);
        bool has_count = (args.size() == 3);
        int count = has_count ? std::stoi(args[2]) : 1;
        
        if (count < 0) {
            throw std::invalid_argument("value is out of range, must be positive");
        }
        
        std::vector<std::string> popped_elements;
        while (!list.empty() && popped_elements.size() < static_cast<size_t>(count)) {
            popped_elements.push_back(list.front());
            list.pop_front();
        }

        if (list.empty()) {
            engine.del(args[1]);
        } else {
            engine.set(args[1], Value(list, val->expire_at));
        }

        if (popped_elements.empty()) {
            return "(nil)\n";
        }

        if (!has_count) {
            return "\"" + popped_elements[0] + "\"\n";
        } else {
            std::string result;
            for (size_t i = 0; i < popped_elements.size(); ++i) {
                result += std::to_string(i + 1) + ") \"" + popped_elements[i] + "\"\n";
            }
            return result;
        }
    }
};

class CmdRPop : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() > 3) throw std::invalid_argument("wrong number of arguments for 'rpop' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(nil)\n";
        require_type<ListType>(val);

        ListType list = std::get<ListType>(val->data);
        bool has_count = (args.size() == 3);
        int count = has_count ? std::stoi(args[2]) : 1;
        
        if (count < 0) {
            throw std::invalid_argument("value is out of range, must be positive");
        }
        
        std::vector<std::string> popped_elements;
        while (!list.empty() && popped_elements.size() < static_cast<size_t>(count)) {
            popped_elements.push_back(list.back());
            list.pop_back();
        }

        if (list.empty()) {
            engine.del(args[1]);
        } else {
            engine.set(args[1], Value(list, val->expire_at));
        }

        if (popped_elements.empty()) {
            return "(nil)\n";
        }

        if (!has_count) {
            return "\"" + popped_elements[0] + "\"\n";
        } else {
            std::string result;
            for (size_t i = 0; i < popped_elements.size(); ++i) {
                result += std::to_string(i + 1) + ") \"" + popped_elements[i] + "\"\n";
            }
            return result;
        }
    }
};

class CmdLLen : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 2) throw std::invalid_argument("wrong number of arguments for 'llen' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n";
        require_type<ListType>(val);

        const ListType& list = std::get<ListType>(val->data);
        return "(integer) " + std::to_string(list.size()) + "\n";
    }
};

class CmdLRange : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 4) throw std::invalid_argument("wrong number of arguments for 'lrange' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(empty array)\n";
        require_type<ListType>(val);

        const ListType& list = std::get<ListType>(val->data);
        long long start = std::stoll(args[2]);
        long long stop = std::stoll(args[3]);
        long long size = list.size();

        if (start < 0) start = std::max(0LL, size + start);
        if (stop < 0) stop = std::max(0LL, size + stop);
        
        std::string result;
        int idx = 1;
        for (long long i = start; i <= stop && i < size; ++i) {
            result += std::to_string(idx++) + ") \"" + list[i] + "\"\n";
        }
        return result.empty() ? "(empty array)\n" : result;
    }
};

class CmdLIndex : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 3) throw std::invalid_argument("wrong number of arguments for 'lindex' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(nil)\n";
        require_type<ListType>(val);

        const ListType& list = std::get<ListType>(val->data);
        long long index = std::stoll(args[2]);
        
        if (index < 0) index += list.size();
        if (index < 0 || static_cast<size_t>(index) >= list.size()) {
            return "(nil)\n"; 
        }

        return "\"" + list[index] + "\"\n";
    }
};


class CmdLSet : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 4) throw std::invalid_argument("wrong number of arguments for 'lset' command");
        
        Value* val = engine.get(args[1]);

        if (!val) throw std::runtime_error("no such key"); 
        require_type<ListType>(val);

        ListType list = std::get<ListType>(val->data);
        long long index = std::stoll(args[2]);
        
        if (index < 0) index += list.size();
        if (index < 0 || static_cast<size_t>(index) >= list.size()) {
            throw std::runtime_error("index out of range"); 
        }

        list[index] = args[3];
        engine.set(args[1], Value(list, val->expire_at));
        
        return "OK\n";
    }
};

class CmdLInsert : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 5) throw std::invalid_argument("wrong number of arguments for 'linsert' command");
        
        Value* val = engine.get(args[1]);
        if (!val) return "(integer) 0\n"; 
        require_type<ListType>(val);

        ListType list = std::get<ListType>(val->data);
        
        std::string direction = args[2];
        std::transform(direction.begin(), direction.end(), direction.begin(), ::toupper);
        
        if (direction != "BEFORE" && direction != "AFTER") {
            throw std::invalid_argument("syntax error");
        }

        const std::string& pivot = args[3];
        const std::string& new_element = args[4];

        auto it = std::find(list.begin(), list.end(), pivot);
        if (it == list.end()) {
            return "(integer) -1\n"; 
        }

        if (direction == "AFTER") {
            ++it;
        }
        
        list.insert(it, new_element);

        size_t size = list.size();
        engine.set(args[1], Value(list, val->expire_at));
        
        return "(integer) " + std::to_string(size) + "\n";
    }
};


void RegisterListCommands(CommandProcessor& processor) {
    processor.register_command("LPUSH", std::make_unique<CmdLPush>());
    processor.register_command("RPUSH", std::make_unique<CmdRPush>());
    processor.register_command("LPOP", std::make_unique<CmdLPop>());
    processor.register_command("RPOP", std::make_unique<CmdRPop>());
    processor.register_command("LLEN", std::make_unique<CmdLLen>());
    processor.register_command("LRANGE", std::make_unique<CmdLRange>());
    processor.register_command("LINDEX", std::make_unique<CmdLIndex>());
    processor.register_command("LSET", std::make_unique<CmdLSet>());
    processor.register_command("LINSERT", std::make_unique<CmdLInsert>());
}