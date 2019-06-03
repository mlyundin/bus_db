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
    using StopsContainer = std::set<std::string>;

public:
    Route() = default;

    StopsContainer::iterator begin();
    StopsContainer::iterator end();

    StopsContainer::const_iterator begin() const;
    StopsContainer::const_iterator end() const;

    void SetDB(const DataBase* db);

    int UniqueStops() const;

    virtual int Stops() const = 0;

    virtual DistanceType Distance() const;

    virtual DistanceType LineDistance() const;

    Json::Object toJsonObject() const;

    static std::shared_ptr<Route> ParseRoute(std::string_view route_str);

    static std::shared_ptr<Route> ParseRoute(const Json::Array& data);

protected:
    void ParseFrom(std::string_view input);

    void ParseFrom(const Json::Array& data);

    virtual std::string_view Delimiter() const = 0;

    template<class Iterator, typename Function> DistanceType Distance(
            Iterator begin, Iterator end, Function func) const {
        DistanceType res = { };
        if (auto it_to = begin; db_ && it_to != end) {
            for (auto it_from = it_to++; it_to != end; ++it_to, ++it_from) {
                res += func(*(*it_from), *(*it_to));
            }
        }

        return res;
    }

    StopsContainer stops_;
    std::list<StopsContainer::const_iterator> route_;
    const DataBase* db_ = nullptr;
};

std::ostream& operator<<(std::ostream& out, const Route& r);

}
