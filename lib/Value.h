#pragma once

#include <string>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <ctime>

struct GeoPoint {
    double longitude;
    double latitude;
};


using StringType = std::string;
using ListType   = std::deque<std::string>;
using SetType    = std::unordered_set<std::string>;
using GeoType    = std::unordered_map<std::string, GeoPoint>;


using ValueData = std::variant<StringType, ListType, SetType, GeoType>;


template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct Value {
    ValueData data;             
    time_t expire_at = 0;          
    size_t estimated_memory = 0;    

    Value() : data(StringType{}) {}

    template <typename T>
    requires (!std::same_as<std::remove_cvref_t<T>, Value>)
    Value(T&& val, time_t ttl = 0) 
        : data(std::forward<T>(val)), expire_at(ttl) 
    {
        update_memory();
    }

    void update_memory() {
        estimated_memory = std::visit(overloaded{
            [](const StringType& s) -> size_t { 
                return s.capacity() + 32; 
            },
            [](const ListType& l) -> size_t { 
                size_t mem = 48;
                for (const auto& str : l) {
                    mem += str.capacity() + 32;
                }
                return mem; 
            },
            [](const SetType& s) -> size_t {
                size_t mem = s.bucket_count() * 8; 
                for (const auto& str : s) {
                    mem += str.capacity() + 32 + 24; 
                }
                return mem;
            },
            [](const GeoType& g) -> size_t {
                size_t mem = g.bucket_count() * 8;
                for (const auto& [name, point] : g) {
                    mem += name.capacity() + sizeof(GeoPoint) + 32 + 24;
                }
                return mem;
            }
        }, data);
    }

    std::string get_type_name() const {
        return std::visit(overloaded{
            [](const StringType&) { return "string"; },
            [](const ListType&)   { return "list"; },
            [](const SetType&)    { return "set"; },
            [](const GeoType&)    { return "zset"; } 
        }, data);
    }
};