#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <sstream>

#include "lib/Engine.h"
#include "lib/Command_Proc.h"

class DatabaseTest : public ::testing::Test {
protected:
    Engine engine;
    CommandProcessor processor;
    
    void SetUp() override {
        processor.process(engine, "FLUSHDB");
        processor.process(engine, "CONFIG SET maxmemory 0");
    }
    long long get_int(const std::string& res) {
        std::regex re(R"(\(integer\)\s+(-?\d+))");
        std::smatch m;
        if (std::regex_search(res, m, re)) {
            return std::stoll(m[1]);
        }
        return 0;
    }

    std::optional<std::string> get_string(const std::string& res) {
        if (res == "(nil)\n" || res.empty()) return std::nullopt;
        std::string s = res;
        if (s.back() == '\n') s.pop_back();
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    std::vector<std::string> get_array(const std::string& res) {
        std::vector<std::string> out;
        if (res == "(empty array)\n" || res == "(nil)\n" || res.empty()) return out;
        
        std::istringstream iss(res);
        std::string line;
        std::regex re(R"raw(^\s*\d+\)\s*"(.*)"\s*$)raw");
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::smatch m;
            if (std::regex_match(line, m, re)) {
                out.push_back(m[1]);
            }
        }
        return out;
    }
};


TEST_F(DatabaseTest, String_SetGetBasic) {
    processor.process(engine, "SET key1 value1");
    EXPECT_EQ(get_string(processor.process(engine, "GET key1")).value_or(""), "value1");
}

TEST_F(DatabaseTest, String_SetOverwrite) {
    processor.process(engine, "SET key1 value1");
    processor.process(engine, "SET key1 value2");
    EXPECT_EQ(get_string(processor.process(engine, "GET key1")).value_or(""), "value2");
}

TEST_F(DatabaseTest, String_GetNonExistent) {
    EXPECT_FALSE(get_string(processor.process(engine, "GET non_existent")).has_value());
}

TEST_F(DatabaseTest, String_StrlenBasic) {
    processor.process(engine, "SET key1 hello");
    EXPECT_EQ(get_int(processor.process(engine, "STRLEN key1")), 5);
}

TEST_F(DatabaseTest, String_StrlenEmptyString) {
    processor.process(engine, "SET key1 \"\"");
    EXPECT_EQ(get_int(processor.process(engine, "STRLEN key1")), 0);
}

TEST_F(DatabaseTest, String_StrlenNonExistent) {
    EXPECT_EQ(get_int(processor.process(engine, "STRLEN non_existent")), 0);
}

TEST_F(DatabaseTest, String_AppendToExisting) {
    processor.process(engine, "SET key1 hello");
    processor.process(engine, "APPEND key1 \" world\""); 
    EXPECT_EQ(get_string(processor.process(engine, "GET key1")).value_or(""), "hello world");
    EXPECT_EQ(get_int(processor.process(engine, "STRLEN key1")), 11);
}

TEST_F(DatabaseTest, String_AppendToNonExistent) {
    processor.process(engine, "APPEND key2 new_string");
    EXPECT_EQ(get_string(processor.process(engine, "GET key2")).value_or(""), "new_string");
}

TEST_F(DatabaseTest, String_ExpireRemovesKey) {
    processor.process(engine, "SET temp val");
    processor.process(engine, "EXPIRE temp 1");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(get_int(processor.process(engine, "EXISTS temp")), 0);
}

TEST_F(DatabaseTest, String_ExpireUpdateTtl) {
    processor.process(engine, "SET temp val");
    processor.process(engine, "EXPIRE temp 5");
    processor.process(engine, "EXPIRE temp 1");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(get_int(processor.process(engine, "EXISTS temp")), 0);
}

TEST_F(DatabaseTest, String_TtlBasic) {
    processor.process(engine, "SET temp val");
    processor.process(engine, "EXPIRE temp 10");
    long long ttl = get_int(processor.process(engine, "TTL temp"));
    EXPECT_LE(ttl, 10);
    EXPECT_GT(ttl, 0);
}

TEST_F(DatabaseTest, String_TtlNoExpire) {
    processor.process(engine, "SET perm val");
    EXPECT_EQ(get_int(processor.process(engine, "TTL perm")), -1);
}

TEST_F(DatabaseTest, String_TtlNonExistent) {
    EXPECT_EQ(get_int(processor.process(engine, "TTL ghost")), -2);
}

TEST_F(DatabaseTest, List_LpushBasic) {
    processor.process(engine, "LPUSH mylist A");
    EXPECT_EQ(get_int(processor.process(engine, "LLEN mylist")), 1);
}

TEST_F(DatabaseTest, List_LpushMultiple) {
    processor.process(engine, "LPUSH mylist A B C"); 
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 0")), "C");
}

TEST_F(DatabaseTest, List_RpushBasic) {
    processor.process(engine, "RPUSH mylist A");
    processor.process(engine, "RPUSH mylist B");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 1")), "B");
}

TEST_F(DatabaseTest, List_RpushMultiple) {
    processor.process(engine, "RPUSH mylist A B C"); 
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 2")), "C");
}

TEST_F(DatabaseTest, List_LpopBasic) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "LPOP mylist 1"));
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0], "A");
    EXPECT_EQ(get_int(processor.process(engine, "LLEN mylist")), 2);
}

TEST_F(DatabaseTest, List_LpopMultiple) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "LPOP mylist 2"));
    ASSERT_EQ(res.size(), 2);
    EXPECT_EQ(res[0], "A");
    EXPECT_EQ(res[1], "B");
}

TEST_F(DatabaseTest, List_LpopEmpty) {
    auto res = get_array(processor.process(engine, "LPOP non_existent 1"));
    EXPECT_TRUE(res.empty());
}

TEST_F(DatabaseTest, List_RpopBasic) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "RPOP mylist 1"));
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0], "C");
}

TEST_F(DatabaseTest, List_RpopMultiple) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "RPOP mylist 2"));
    ASSERT_EQ(res.size(), 2);
    EXPECT_EQ(res[0], "C");
    EXPECT_EQ(res[1], "B");
}

TEST_F(DatabaseTest, List_LlenBasic) {
    EXPECT_EQ(get_int(processor.process(engine, "LLEN mylist")), 0);
    processor.process(engine, "RPUSH mylist A");
    EXPECT_EQ(get_int(processor.process(engine, "LLEN mylist")), 1);
}

TEST_F(DatabaseTest, List_LrangeFull) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "LRANGE mylist 0 -1"));
    EXPECT_EQ(res, (std::vector<std::string>{"A", "B", "C"}));
}

TEST_F(DatabaseTest, List_LrangePartial) {
    processor.process(engine, "RPUSH mylist A B C D");
    auto res = get_array(processor.process(engine, "LRANGE mylist 1 2"));
    EXPECT_EQ(res, (std::vector<std::string>{"B", "C"}));
}

TEST_F(DatabaseTest, List_LrangeNegativeIndices) {
    processor.process(engine, "RPUSH mylist A B C D");
    auto res = get_array(processor.process(engine, "LRANGE mylist -3 -2"));
    EXPECT_EQ(res, (std::vector<std::string>{"B", "C"}));
}

TEST_F(DatabaseTest, List_LrangeOutOfBounds) {
    processor.process(engine, "RPUSH mylist A B C");
    auto res = get_array(processor.process(engine, "LRANGE mylist 1 100"));
    EXPECT_EQ(res, (std::vector<std::string>{"B", "C"}));
}

TEST_F(DatabaseTest, List_LindexValid) {
    processor.process(engine, "RPUSH mylist A B C");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 1")), "B");
}

TEST_F(DatabaseTest, List_LindexNegative) {
    processor.process(engine, "RPUSH mylist A B C");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist -1")), "C");
}

TEST_F(DatabaseTest, List_LindexOutOfBounds) {
    processor.process(engine, "RPUSH mylist A");
    EXPECT_FALSE(get_string(processor.process(engine, "LINDEX mylist 10")).has_value()); 
}

TEST_F(DatabaseTest, List_LsetValid) {
    processor.process(engine, "RPUSH mylist A B");
    processor.process(engine, "LSET mylist 1 X");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 1")), "X");
}

TEST_F(DatabaseTest, List_LsetOutOfBounds) {
    processor.process(engine, "RPUSH mylist A");
    std::string res = processor.process(engine, "LSET mylist 5 X");
    EXPECT_TRUE(res.find("(error)") != std::string::npos); 
}

TEST_F(DatabaseTest, List_LinsertAfter) {
    processor.process(engine, "RPUSH mylist A B D");
    processor.process(engine, "LINSERT mylist AFTER B C");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 2")), "C");
}

TEST_F(DatabaseTest, List_LinsertBefore) {
    processor.process(engine, "RPUSH mylist A C D");
    processor.process(engine, "LINSERT mylist BEFORE C B");
    EXPECT_EQ(get_string(processor.process(engine, "LINDEX mylist 1")), "B");
}

TEST_F(DatabaseTest, Set_SaddBasic) {
    EXPECT_EQ(get_int(processor.process(engine, "SADD myset A B")), 2);
    EXPECT_EQ(get_int(processor.process(engine, "SCARD myset")), 2);
}

TEST_F(DatabaseTest, Set_SaddDuplicates) {
    EXPECT_EQ(get_int(processor.process(engine, "SADD myset A A B")), 2);
    EXPECT_EQ(get_int(processor.process(engine, "SADD myset B C")), 1);
    EXPECT_EQ(get_int(processor.process(engine, "SCARD myset")), 3);
}

TEST_F(DatabaseTest, Set_SremBasic) {
    processor.process(engine, "SADD myset A B C");
    EXPECT_EQ(get_int(processor.process(engine, "SREM myset A C")), 2);
    EXPECT_EQ(get_int(processor.process(engine, "SCARD myset")), 1);
}

TEST_F(DatabaseTest, Set_SremNonExistent) {
    processor.process(engine, "SADD myset A");
    EXPECT_EQ(get_int(processor.process(engine, "SREM myset B")), 0);
}

TEST_F(DatabaseTest, Set_SismemberValid) {
    processor.process(engine, "SADD myset A");
    EXPECT_EQ(get_int(processor.process(engine, "SISMEMBER myset A")), 1);
    EXPECT_EQ(get_int(processor.process(engine, "SISMEMBER myset B")), 0);
}

TEST_F(DatabaseTest, Set_SmembersBasic) {
    processor.process(engine, "SADD myset A B C");
    auto members = get_array(processor.process(engine, "SMEMBERS myset"));
    std::unordered_set<std::string> mset(members.begin(), members.end());
    EXPECT_EQ(mset.size(), 3);
    EXPECT_TRUE(mset.count("A") && mset.count("B") && mset.count("C"));
}

TEST_F(DatabaseTest, Set_ScardEmpty) {
    EXPECT_EQ(get_int(processor.process(engine, "SCARD non_existent")), 0);
}

TEST_F(DatabaseTest, Set_SunionTwoSets) {
    processor.process(engine, "SADD set1 A B");
    processor.process(engine, "SADD set2 B C");
    auto res = get_array(processor.process(engine, "SUNION set1 set2"));
    EXPECT_EQ(res.size(), 3);
}

TEST_F(DatabaseTest, Set_SunionMultiple) {
    processor.process(engine, "SADD s1 1"); 
    processor.process(engine, "SADD s2 2"); 
    processor.process(engine, "SADD s3 3");
    auto res = get_array(processor.process(engine, "SUNION s1 s2 s3"));
    EXPECT_EQ(res.size(), 3);
}

TEST_F(DatabaseTest, Set_SinterTwoSets) {
    processor.process(engine, "SADD set1 A B C");
    processor.process(engine, "SADD set2 B C D");
    auto res = get_array(processor.process(engine, "SINTER set1 set2"));
    EXPECT_EQ(res.size(), 2);
    std::unordered_set<std::string> mset(res.begin(), res.end());
    EXPECT_TRUE(mset.count("B") && mset.count("C"));
}

TEST_F(DatabaseTest, Set_SinterEmpty) {
    processor.process(engine, "SADD set1 A");
    processor.process(engine, "SADD set2 B");
    auto res = get_array(processor.process(engine, "SINTER set1 set2"));
    EXPECT_TRUE(res.empty());
}

TEST_F(DatabaseTest, Set_SdiffTwoSets) {
    processor.process(engine, "SADD set1 A B C");
    processor.process(engine, "SADD set2 B D");
    auto res = get_array(processor.process(engine, "SDIFF set1 set2"));
    EXPECT_EQ(res.size(), 2);
}

TEST_F(DatabaseTest, Set_SdiffMultipleSets) {
    processor.process(engine, "SADD set1 A B C D");
    processor.process(engine, "SADD set2 B");
    processor.process(engine, "SADD set3 C E");
    auto res = get_array(processor.process(engine, "SDIFF set1 set2 set3"));
    EXPECT_EQ(res.size(), 2);
}

TEST_F(DatabaseTest, Set_SdiffNonExistent) {
    processor.process(engine, "SADD set1 A");
    auto res = get_array(processor.process(engine, "SDIFF set1 ghost_set"));
    EXPECT_EQ(res.size(), 1);
}

TEST_F(DatabaseTest, Set_SmoveValid) {
    processor.process(engine, "SADD src A B");
    processor.process(engine, "SADD dst C");
    EXPECT_EQ(get_int(processor.process(engine, "SMOVE src dst A")), 1);
    EXPECT_EQ(get_int(processor.process(engine, "SISMEMBER src A")), 0);
    EXPECT_EQ(get_int(processor.process(engine, "SISMEMBER dst A")), 1);
}

TEST_F(DatabaseTest, Set_SmoveNonExistentMember) {
    processor.process(engine, "SADD src A");
    EXPECT_EQ(get_int(processor.process(engine, "SMOVE src dst B")), 0);
}

TEST_F(DatabaseTest, Set_SmoveToSameSet) {
    processor.process(engine, "SADD src A");
    processor.process(engine, "SMOVE src src A");
    EXPECT_EQ(get_int(processor.process(engine, "SISMEMBER src A")), 1);
}

TEST_F(DatabaseTest, Geo_AddBasic) {
    EXPECT_EQ(get_int(processor.process(engine, "GEOADD sicily 13.361389 38.115556 Palermo")), 1);
}

TEST_F(DatabaseTest, Geo_AddUpdate) {
    processor.process(engine, "GEOADD sicily 10.0 10.0 Palermo");
    EXPECT_EQ(get_int(processor.process(engine, "GEOADD sicily 13.361389 38.115556 Palermo")), 0);
}

TEST_F(DatabaseTest, Geo_PosBasic) {
    processor.process(engine, "GEOADD sicily 13.361389 38.115556 Palermo");
    std::string res = processor.process(engine, "GEOPOS sicily Palermo");
    EXPECT_TRUE(res.find("13.361389") != std::string::npos);
    EXPECT_TRUE(res.find("38.115556") != std::string::npos);
}

TEST_F(DatabaseTest, Geo_PosNonExistent) {
    std::string res = processor.process(engine, "GEOPOS sicily Rome");
    EXPECT_TRUE(res.find("(nil)") != std::string::npos);
}

TEST_F(DatabaseTest, Geo_DistKm) {
    processor.process(engine, "GEOADD sicily 13.361389 38.115556 Palermo");
    processor.process(engine, "GEOADD sicily 15.087269 37.502669 Catania");
    std::string res = processor.process(engine, "GEODIST sicily Palermo Catania km");
    double dist = std::stod(*get_string(res));
    EXPECT_NEAR(dist, 166.0, 1.0);
}

TEST_F(DatabaseTest, Geo_DistMeters) {
    processor.process(engine, "GEOADD pts 0.0 0.0 p1");
    processor.process(engine, "GEOADD pts 0.0 0.01 p2");
    std::string res = processor.process(engine, "GEODIST pts p1 p2 m");
    double dist = std::stod(*get_string(res));
    EXPECT_NEAR(dist, 1112.0, 5.0);
}

TEST_F(DatabaseTest, Geo_DistMiles) {
    processor.process(engine, "GEOADD sicily 13.361389 38.115556 Palermo");
    processor.process(engine, "GEOADD sicily 15.087269 37.502669 Catania");
    std::string res = processor.process(engine, "GEODIST sicily Palermo Catania mi");
    double dist = std::stod(*get_string(res));
    EXPECT_NEAR(dist, 103.0, 1.0);
}

TEST_F(DatabaseTest, Geo_DistSamePoint) {
    processor.process(engine, "GEOADD pts 10.0 10.0 p1");
    std::string res = processor.process(engine, "GEODIST pts p1 p1 km");
    double dist = std::stod(*get_string(res));
    EXPECT_EQ(dist, 0.0);
}

TEST_F(DatabaseTest, Geo_SearchBasic) {
    processor.process(engine, "GEOADD cities 37.6173 55.7558 Moscow");
    processor.process(engine, "GEOADD cities 30.3141 59.9386 SPB");
    processor.process(engine, "GEOADD cities 39.7390 47.2313 Rostov");
    
    auto res = get_array(processor.process(engine, "GEOSEARCH cities FROMLONLAT 37.6173 55.7558 BYRADIUS 800 km"));
    std::unordered_set<std::string> res_set(res.begin(), res.end());
    EXPECT_TRUE(res_set.count("Moscow"));
    EXPECT_TRUE(res_set.count("SPB"));
    EXPECT_FALSE(res_set.count("Rostov"));
}

TEST_F(DatabaseTest, Geo_SearchOrderAsc) {
    processor.process(engine, "GEOADD cities 37.6173 55.7558 Moscow");
    processor.process(engine, "GEOADD cities 30.3141 59.9386 SPB");
    processor.process(engine, "GEOADD cities 37.5000 55.7000 Podolsk");

    auto res = get_array(processor.process(engine, "GEOSEARCH cities FROMLONLAT 37.6173 55.7558 BYRADIUS 1000 km ASC"));
    ASSERT_EQ(res.size(), 3);
    EXPECT_EQ(res[0], "Moscow");
    EXPECT_EQ(res[1], "Podolsk");
    EXPECT_EQ(res[2], "SPB");
}

TEST_F(DatabaseTest, Geo_SearchCountLimit) {
    processor.process(engine, "GEOADD cities 37.6173 55.7558 Moscow");
    processor.process(engine, "GEOADD cities 30.3141 59.9386 SPB");
    
    auto res = get_array(processor.process(engine, "GEOSEARCH cities FROMLONLAT 37.6173 55.7558 BYRADIUS 1000 km COUNT 1"));
    EXPECT_EQ(res.size(), 1);
}

TEST_F(DatabaseTest, Geo_SearchStore) {
    processor.process(engine, "GEOADD cities 37.6173 55.7558 Moscow");
    processor.process(engine, "GEOADD cities 30.3141 59.9386 SPB");
    
    processor.process(engine, "GEOSEARCHSTORE results_geo cities FROMLONLAT 37.6173 55.7558 BYRADIUS 800 km");
    std::string t = processor.process(engine, "TYPE results_geo");
    if (t.back() == '\n') t.pop_back();
    EXPECT_EQ(t, "list");
}

TEST_F(DatabaseTest, Gen_TypeChecks) {
    processor.process(engine, "SET s v");
    processor.process(engine, "LPUSH l v");
    processor.process(engine, "SADD set v");
    processor.process(engine, "GEOADD g 0.0 0.0 v");

    auto get_t = [&](const std::string& k) {
        std::string t = processor.process(engine, "TYPE " + k);
        if (t.back() == '\n') t.pop_back();
        return t;
    };

    EXPECT_EQ(get_t("s"), "string");
    EXPECT_EQ(get_t("l"), "list");
    EXPECT_EQ(get_t("set"), "set");
    EXPECT_EQ(get_t("g"), "zset"); 
    EXPECT_EQ(get_t("ghost"), "none");
}

TEST_F(DatabaseTest, Gen_DelBasic) {
    processor.process(engine, "SET k1 v");
    processor.process(engine, "SET k2 v");
    EXPECT_EQ(get_int(processor.process(engine, "DEL k1 k2 ghost")), 2);
    EXPECT_EQ(get_int(processor.process(engine, "EXISTS k1")), 0);
}

TEST_F(DatabaseTest, Gen_ExistsMultiple) {
    processor.process(engine, "SET k1 v");
    processor.process(engine, "SET k2 v");
    EXPECT_EQ(get_int(processor.process(engine, "EXISTS k1 k2 k3")), 2);
}

TEST_F(DatabaseTest, Gen_KeysPatternAll) {
    processor.process(engine, "SET key1 v");
    processor.process(engine, "SET key2 v");
    auto keys = get_array(processor.process(engine, "KEYS *"));
    EXPECT_EQ(keys.size(), 2);
}

TEST_F(DatabaseTest, Gen_FlushDb) {
    processor.process(engine, "SET k1 v");
    processor.process(engine, "LPUSH l1 v");
    processor.process(engine, "FLUSHDB");
    EXPECT_EQ(get_int(processor.process(engine, "DBSIZE")), 0);
}

TEST_F(DatabaseTest, Gen_DbSize) {
    EXPECT_EQ(get_int(processor.process(engine, "DBSIZE")), 0);
    processor.process(engine, "SET k1 v");
    EXPECT_EQ(get_int(processor.process(engine, "DBSIZE")), 1);
}

TEST_F(DatabaseTest, Gen_CaseSensitiveKeys) {
    processor.process(engine, "SET KEY upper");
    processor.process(engine, "SET key lower");
    EXPECT_EQ(get_string(processor.process(engine, "GET KEY")).value_or(""), "upper");
    EXPECT_EQ(get_string(processor.process(engine, "GET key")).value_or(""), "lower");
}

TEST_F(DatabaseTest, Gen_WrongTypeOperation) {
    processor.process(engine, "SET string_key val");
    std::string res = processor.process(engine, "LPUSH string_key v");
    EXPECT_TRUE(res.find("(error)") != std::string::npos); 
}

TEST_F(DatabaseTest, Mem_ConfigSetGet) {
    processor.process(engine, "CONFIG SET maxmemory 1024");
    auto res = get_array(processor.process(engine, "CONFIG GET maxmemory"));
    ASSERT_EQ(res.size(), 2);
    EXPECT_EQ(std::stoull(res[1]), 1024);
    processor.process(engine, "CONFIG SET maxmemory 0"); 
}

TEST_F(DatabaseTest, Mem_UsageIsGreaterThanData) {
    processor.process(engine, "SET k1 v");
    EXPECT_GT(get_int(processor.process(engine, "MEMORY USAGE k1")), 3);
}

TEST_F(DatabaseTest, Mem_MemoryDecreasesOnDel) {
    std::string big_str(50000, 'X');
    processor.process(engine, "SET big_key " + big_str);
    
    size_t mem_with_key = engine.get_used_memory();
    processor.process(engine, "DEL big_key");
    size_t mem_without_key = engine.get_used_memory();
    
    EXPECT_LT(mem_without_key, mem_with_key);
    EXPECT_GE(mem_with_key - mem_without_key, 50000);
}

TEST_F(DatabaseTest, Mem_OomOnSet) {
    processor.process(engine, "CONFIG SET maxmemory 100");
    std::string big_str(200, 'A');
    
    std::string res = processor.process(engine, "SET k1 " + big_str);
    EXPECT_TRUE(res.find("(error)") != std::string::npos); 
    EXPECT_EQ(get_int(processor.process(engine, "DBSIZE")), 0); 
}

TEST_F(DatabaseTest, Mem_OomOnListPush) {
    processor.process(engine, "CONFIG SET maxmemory 500"); 
    processor.process(engine, "LPUSH mylist A"); 
    
    std::string big_str(1000, 'B');
    std::string res = processor.process(engine, "LPUSH mylist " + big_str);
    EXPECT_TRUE(res.find("(error)") != std::string::npos); 
    EXPECT_EQ(get_int(processor.process(engine, "LLEN mylist")), 1); 
}