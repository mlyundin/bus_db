#pragma once

#include<set>
#include<iostream>
#include<list>
#include<memory>
#include<string_view>

#include "common.h"
#include "json.h"

namespace busdb {

class DataBase;

class Route {
protected:
    using StopsContainer = std::set<std::string>;

public:
    Route() = default;

    void SetDB(const DataBase* db);

    const StopsContainer& UniqueStops() const;

    const std::list<StopsContainer::const_iterator>& Stops() const;

    DistanceType Distance() const;

    DistanceType LineDistance() const;

    Json::Object ToJsonObject() const;

    virtual std::array<StopsContainer::const_iterator, 2> EdgeStops() const = 0;

    static std::unique_ptr<Route> ParseRoute(std::string_view route_str);

    static std::unique_ptr<Route> ParseRoute(const Json::Object& data);

    virtual ~Route() = default;

protected:
    void ParseFrom(std::string_view input);

    void ParseFrom(const Json::Object& data);

    virtual std::string_view Delimiter() const = 0;

    virtual void FillRoute() = 0;

    StopsContainer stops_;
    std::list<StopsContainer::const_iterator> route_;
    const DataBase* db_ = nullptr;
};

std::ostream& operator<<(std::ostream& out, const Route& r);

}
