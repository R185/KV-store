#include "Command_Proc.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <stdexcept>

constexpr double EARTH_RADIUS_KM = 6372.8;

struct GeoSearchResult {
    std::string member;
    double distance;
};

inline double unit_to_km(const std::string& unit) {
    if (unit == "m") return 0.001;
    if (unit == "km") return 1.0;
    if (unit == "mi") return 1.60934;
    if (unit == "ft") return 0.0003048;
    throw std::invalid_argument("unsupported unit");
}

inline double haversine(double lon1, double lat1, double lon2, double lat2) {
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = std::pow(std::sin(dLat / 2), 2) + 
               std::pow(std::sin(dLon / 2), 2) * std::cos(lat1) * std::cos(lat2);
    double c = 2 * std::asin(std::sqrt(a));
    return EARTH_RADIUS_KM * c;
}

inline std::vector<GeoSearchResult> perform_geosearch(
    const GeoType& geo, 
    double target_lon, 
    double target_lat, 
    double radius_km, 
    const std::string& sort_order,
    int count_limit                
) {
    std::vector<GeoSearchResult> found;
    for (const auto& [member, point] : geo) {
        double dist = haversine(target_lon, target_lat, point.longitude, point.latitude);
        if (dist <= radius_km) {
            found.push_back({member, dist});
        }
    }

    if (sort_order == "ASC") {
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
            return a.distance < b.distance;
        });
    } else if (sort_order == "DESC") {
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
            return a.distance > b.distance;
        });
    }

    if (count_limit > 0 && static_cast<size_t>(count_limit) < found.size()) {
        found.resize(count_limit);
    }

    return found;
}

inline void parse_search_options(
    const std::vector<std::string>& args, 
    size_t start_idx, 
    std::string& sort_order, 
    int& count_limit
) {
    size_t idx = start_idx;
    while (idx < args.size()) {
        std::string opt = args[idx];
        std::transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
        
        if (opt == "ASC" || opt == "DESC") {
            sort_order = opt;
            idx++;
        } else if (opt == "COUNT" && idx + 1 < args.size()) {
            count_limit = std::stoi(args[idx + 1]);
            idx += 2;
        } else {
            throw std::invalid_argument("syntax error in search options");
        }
    }
}

class CmdGeoAdd : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 5 || (args.size() - 2) % 3 != 0) 
            throw std::invalid_argument("wrong number of arguments for 'geoadd' command");
            
        Value* val = engine.get(args[1]);
        require_type<GeoType>(val);

        GeoType geo = val ? std::get<GeoType>(val->data) : GeoType{};
        time_t ttl = val ? val->expire_at : 0;
        int added = 0;

        for (size_t i = 2; i < args.size(); i += 3) {
            double lon = std::stod(args[i]);
            double lat = std::stod(args[i+1]);
            std::string member = args[i+2];
            
            if (geo.find(member) == geo.end()) {
                added++;
            }
            geo[member] = {lon, lat};
        }
        
        engine.set(args[1], Value(geo, ttl));
        return "(integer) " + std::to_string(added) + "\n";
    }
};

class CmdGeoPos : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 3) throw std::invalid_argument("wrong number of arguments for 'geopos' command");
        
        Value* val = engine.get(args[1]);
        if (val) require_type<GeoType>(val);

        std::string result;
        int idx = 1;
        for (size_t i = 2; i < args.size(); ++i) {
            result += std::to_string(idx++) + ") ";
            if (!val) {
                result += "(nil)\n";
                continue;
            }

            const auto& geo = std::get<GeoType>(val->data);
            auto it = geo.find(args[i]);
            if (it == geo.end()) {
                result += "(nil)\n";
            } else {
                result += "1) \"" + std::to_string(it->second.longitude) + "\"\n" +
                          "   2) \"" + std::to_string(it->second.latitude) + "\"\n";
            }
        }
        return result;
    }
};

class CmdGeoDist : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() != 4 && args.size() != 5) 
            throw std::invalid_argument("wrong number of arguments for 'geodist' command");
            
        Value* val = engine.get(args[1]);
        if (!val) return "(nil)\n";
        require_type<GeoType>(val);

        const GeoType& geo = std::get<GeoType>(val->data);
        auto it1 = geo.find(args[2]);
        auto it2 = geo.find(args[3]);
        if (it1 == geo.end() || it2 == geo.end()) {
            return "(nil)\n";
        }

        std::string unit = args.size() == 5 ? args[4] : "m";
        double multiplier = unit_to_km(unit);

        double dist_km = haversine(it1->second.longitude, it1->second.latitude, 
                                   it2->second.longitude, it2->second.latitude);
        
        return "\"" + std::to_string(dist_km / multiplier) + "\"\n";
    }
};


class CmdGeoSearch : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 8) 
            throw std::invalid_argument("wrong number of arguments for 'geosearch' command");

        if (args[2] != "FROMLONLAT" || args[5] != "BYRADIUS") {
            throw std::invalid_argument("syntax error in geosearch options");
        }

        Value* val = engine.get(args[1]);
        if (!val) return "(empty array)\n";
        require_type<GeoType>(val);
        const GeoType& geo = std::get<GeoType>(val->data);

        double target_lon = std::stod(args[3]);
        double target_lat = std::stod(args[4]);
        double radius = std::stod(args[6]);
        std::string unit = args[7];

        double radius_km = radius * unit_to_km(unit);

        std::string sort_order = "";
        int count_limit = -1;
        parse_search_options(args, 8, sort_order, count_limit);

        auto search_results = perform_geosearch(geo, target_lon, target_lat, radius_km, sort_order, count_limit);

        if (search_results.empty()) {
            return "(empty array)\n";
        }

        std::string output;
        int idx = 1;
        for (const auto& res : search_results) {
            output += std::to_string(idx++) + ") \"" + res.member + "\"\n";
        }
        return output;
    }
};

class CmdGeoSearchStore : public Command {
public:
    std::string execute(Engine& engine, const std::vector<std::string>& args) override {
        if (args.size() < 9) 
            throw std::invalid_argument("wrong number of arguments for 'geosearchstore' command");

        if (args[3] != "FROMLONLAT" || args[6] != "BYRADIUS") {
            throw std::invalid_argument("syntax error in geosearchstore options");
        }

        std::string dest_key = args[1];
        std::string source_key = args[2];

        Value* source_val = engine.get(source_key);
        if (!source_val) {
            engine.set(dest_key, Value(ListType{}));
            return "(integer) 0\n";
        }
        require_type<GeoType>(source_val);
        const GeoType& geo = std::get<GeoType>(source_val->data);

        double target_lon = std::stod(args[4]);
        double target_lat = std::stod(args[5]);
        double radius = std::stod(args[7]);
        std::string unit = args[8];

        double radius_km = radius * unit_to_km(unit);

        std::string sort_order = "";
        int count_limit = -1;
        parse_search_options(args, 9, sort_order, count_limit);

        auto search_results = perform_geosearch(geo, target_lon, target_lat, radius_km, sort_order, count_limit);

        ListType dest_list;
        for (const auto& res : search_results) {
            dest_list.push_back(res.member);
        }

        engine.set(dest_key, Value(dest_list));

        return "(integer) " + std::to_string(dest_list.size()) + "\n";
    }
};

void RegisterGeoCommands(CommandProcessor& processor) {
    processor.register_command("GEOADD", std::make_unique<CmdGeoAdd>());
    processor.register_command("GEOPOS", std::make_unique<CmdGeoPos>());
    processor.register_command("GEODIST", std::make_unique<CmdGeoDist>());
    processor.register_command("GEOSEARCH", std::make_unique<CmdGeoSearch>());
    processor.register_command("GEOSEARCHSTORE", std::make_unique<CmdGeoSearchStore>());
}