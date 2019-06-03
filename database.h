#pragma once

#include<set>
#include<unordered_map>
#include<string>
#include<string_view>
#include<optional>
#include<memory>

#include "common.h"

namespace busdb {

class Route;

class DataBase {
public:
    DataBase() = default;

    DistanceType Distance(const std::string& stop1,
            const std::string& stop2) const;

    DistanceType LineDistance(const std::string& stop1,
            const std::string& stop2) const;

    void AddStop(std::string stop, Point location,
            const std::unordered_map<std::string, int>& distances);

    void AddBus(std::string number, std::shared_ptr<Route> route);

    std::shared_ptr<Route> GetBusRoute(const std::string& number) const;

    std::optional<std::set<std::string_view>> GetStopBuses(
            std::string_view stop) const;

private:
    std::unordered_map<std::string, Point> stops_;
    std::unordered_map<std::string, std::shared_ptr<Route>> buses_;
    std::unordered_map<std::string_view, std::set<std::string_view>> stop_buses_;
    mutable std::unordered_map<std::string_view,
            std::unordered_map<std::string_view, DistanceType>> distance_hash_;
};

}
