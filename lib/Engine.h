#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>

#include "Value.h" 

class OOMError : public std::runtime_error {
public:
    OOMError() : std::runtime_error("(error) OOM command not allowed when used memory > 'maxmemory'") {}
};

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void set_max_memory(size_t bytes);
    size_t get_max_memory() const;
    size_t get_used_memory() const;

    Value* get(const std::string& key);
    void set(const std::string& key, Value val);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    size_t dbsize();
    void flushdb();
    
    std::vector<std::string> keys(const std::string& pattern);

private:
    std::unordered_map<std::string, Value> keyspace_;
    
    size_t max_memory_ = 0; 
    size_t used_memory_ = 0;


    size_t get_key_overhead(const std::string& key) const;
    void check_oom(size_t additional_bytes) const;
    bool lazy_expire(const std::string& key);
    bool match_pattern(const char* pattern, const char* str) const;
};

inline time_t get_current_time_sec() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}