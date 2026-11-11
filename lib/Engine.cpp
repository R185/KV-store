#include "Engine.h"


void Engine::set_max_memory(size_t bytes) {
    max_memory_ = bytes;
}

size_t Engine::get_max_memory() const {
    return max_memory_;
}

size_t Engine::get_used_memory() const {
    return used_memory_;
}

size_t Engine::get_key_overhead(const std::string& key) const {
    return 32 + key.capacity();
}

void Engine::check_oom(size_t additional_bytes) const {
    if (max_memory_ > 0 && (used_memory_ + additional_bytes) > max_memory_) {
        throw OOMError();
    }
}


bool Engine::lazy_expire(const std::string& key) {
    auto it = keyspace_.find(key);
    if (it == keyspace_.end()) {
        return false; 
    }

    if (it->second.expire_at > 0 && it->second.expire_at <= get_current_time_sec()) {
        size_t mem_to_free = get_key_overhead(it->first) + it->second.estimated_memory;
        used_memory_ -= mem_to_free;
        keyspace_.erase(it);
        return true;
    }

    return false;
}



Value* Engine::get(const std::string& key) {
    if (lazy_expire(key)) {
        return nullptr;
    }

    auto it = keyspace_.find(key);
    if (it != keyspace_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void Engine::set(const std::string& key, Value val) {

    lazy_expire(key);

    size_t new_val_mem = val.estimated_memory;
    size_t key_overhead = get_key_overhead(key);
    size_t total_new_mem = key_overhead + new_val_mem;

    auto it = keyspace_.find(key);
    if (it != keyspace_.end()) {

        size_t old_val_mem = it->second.estimated_memory;
        size_t total_old_mem = key_overhead + old_val_mem;

        if (total_new_mem > total_old_mem) {
            check_oom(total_new_mem - total_old_mem);
            used_memory_ += (total_new_mem - total_old_mem);
        } else {
            used_memory_ -= (total_old_mem - total_new_mem);
        }

        it->second = std::move(val);
    } else {

        check_oom(total_new_mem);
        used_memory_ += total_new_mem;
        keyspace_.emplace(key, std::move(val));
    }
}

bool Engine::del(const std::string& key) {
    lazy_expire(key); 

    auto it = keyspace_.find(key);
    if (it != keyspace_.end()) {
        size_t mem_to_free = get_key_overhead(it->first) + it->second.estimated_memory;
        used_memory_ -= mem_to_free;
        keyspace_.erase(it);
        return true;
    }
    return false;
}

bool Engine::exists(const std::string& key) {
    if (lazy_expire(key)) {
        return false;
    }
    return keyspace_.find(key) != keyspace_.end();
}



size_t Engine::dbsize() {
    return keyspace_.size();
}

void Engine::flushdb() {
    keyspace_.clear();
    used_memory_ = 0;
}

bool Engine::match_pattern(const char* pattern, const char* str) const {
    if (*pattern == '\0') return *str == '\0';
    if (*pattern == '*') {
        return match_pattern(pattern + 1, str) || (*str != '\0' && match_pattern(pattern, str + 1));
    }
    if (*pattern == '?' && *str != '\0') {
        return match_pattern(pattern + 1, str + 1);
    }
    if (*pattern == *str) {
        return match_pattern(pattern + 1, str + 1);
    }
    return false;
}

std::vector<std::string> Engine::keys(const std::string& pattern) {
    std::vector<std::string> result;

    for (auto it = keyspace_.begin(); it != keyspace_.end(); ) {
        if (it->second.expire_at > 0 && it->second.expire_at <= get_current_time_sec()) {
            size_t mem_to_free = get_key_overhead(it->first) + it->second.estimated_memory;
            used_memory_ -= mem_to_free;
            it = keyspace_.erase(it);
        } else {
            if (match_pattern(pattern.c_str(), it->first.c_str())) {
                result.push_back(it->first);
            }
            ++it;
        }
    }
    
    return result;
}