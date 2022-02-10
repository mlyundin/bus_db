#include "route.h"
#include "database.h"

#include <algorithm>

namespace
{
using namespace busdb;

template<class Iterator, typename Function>
DistanceType Distance(Iterator begin, Iterator end, Function func) {
    DistanceType res = { };
    if (auto it_to = begin; it_to != end) {
        for (auto it_from = it_to++; it_to != end; ++it_to, ++it_from) {
            res += func(*(*it_from), *(*it_to));
        }
    }

    return res;
}

class CircleRoute: public Route {
public:
    static std::string delimiter;

    std::array<StopsContainer::const_iterator, 2> EdgeStops() const override {
        return {*std::cbegin(route_), *std::cbegin(route_)};
    }

protected:
    std::string_view Delimiter() const override {
        return CircleRoute::delimiter;
    }

    void FillRoute() override {

    }
};

class TwoWayRoute: public Route {
    StopsContainer::const_iterator last_stop_;
public:
    static std::string delimiter;

    std::array<StopsContainer::const_iterator, 2> EdgeStops() const override {
        return {*std::cbegin(route_), last_stop_};
    }

protected:
    std::string_view Delimiter() const override {
        return TwoWayRoute::delimiter;
    }

    void FillRoute() override {
        last_stop_ = *route_.rbegin();
        if (route_.size() >= 2)
            copy(++route_.rbegin(), route_.rend(), back_inserter(route_));
    }
};
std::string CircleRoute::delimiter = " > ";

std::string TwoWayRoute::delimiter = " - ";
}

namespace busdb {

void Route::ParseFrom(std::string_view stops) {
    auto delimiter = Delimiter();
    while (stops.size()) {
        auto [it, inserted] = stops_.insert(std::string(ReadToken(stops, delimiter)));
        route_.push_back(it);
    }
    FillRoute();
}

void Route::ParseFrom(const Json::Object& data) {
    for (const auto& item : data.at("stops").AsArray()) {
        auto [it, inserted] = stops_.insert(item.AsString());
        route_.push_back(it);
    }
    FillRoute();
}

void Route::SetDB(const DataBase* db) {
    db_ = db;
}

const Route::StopsContainer& Route::UniqueStops() const {
    return stops_;
}

const std::list<Route::StopsContainer::const_iterator>& Route::Stops() const {
    return route_;
}

DistanceType Route::Distance() const {
    return ::Distance(route_.cbegin(), route_.cend(),
                      [this](const auto& from, const auto& to) {return this->db_->Distance(from, to);});
}

DistanceType Route::LineDistance() const {
    return ::Distance(route_.cbegin(), route_.cend(),
                      [this](const auto& from, const auto& to) {return this->db_->LineDistance(from, to);});
}

Json::Object Route::ToJsonObject() const {
    auto distance = Distance();
    Json::Object json = {{"curvature", distance / LineDistance()},
            {"stop_count", (int)Stops().size()},
            {"unique_stop_count", (int)UniqueStops().size()}};

    if (distance - int(distance) > 0) json["route_length"] = distance;
    else json["route_length"] = int(distance);

    return json;
}

std::ostream& operator<<(std::ostream& out, const Route& r) {
    auto distance = r.Distance();
    return out << r.Stops().size() << " stops on route, " << r.UniqueStops().size()
            << " unique stops, " << int(distance) << " route length, "
            << distance / r.LineDistance() << " curvature";
}

std::unique_ptr<Route> Route::ParseRoute(std::string_view route_str) {
    std::unique_ptr<Route> route = nullptr;

    if (route_str.find(CircleRoute::delimiter) != std::string_view::npos) {
        route = std::make_unique<CircleRoute>();
    } else if (route_str.find(TwoWayRoute::delimiter) != std::string_view::npos) {
        route = std::make_unique<TwoWayRoute>();
    }

    if (route)
        route->ParseFrom(route_str);

    return route;
}

std::unique_ptr<Route> Route::ParseRoute(const Json::Object& data) {
    std::unique_ptr<Route> route = nullptr;

    if (data.at("is_roundtrip").AsBoolean()) {
        route = std::make_unique<CircleRoute>();
    } else {
        route = std::make_unique<TwoWayRoute>();
    }

    route->ParseFrom(data);

    return route;
}

}
