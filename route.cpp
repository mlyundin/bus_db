#include "route.h"
#include "database.h"

#include <algorithm>

using namespace std;

namespace busdb {

template<class Iterator, typename Function> DistanceType Distance(
        Iterator begin, Iterator end, Function func) {
    DistanceType res = { };
    if (auto it_to = begin; it_to != end) {
        for (auto it_from = it_to++; it_to != end; ++it_to, ++it_from) {
            res += func(*(*it_from), *(*it_to));
        }
    }

    return res;
}

void Route::ParseFrom(string_view stops) {
    auto delimiter = Delimiter();
    while (stops.size()) {
        auto [it, inserted] = stops_.insert(string(ReadToken(stops, delimiter)));
        route_.push_back(it);
    }
    FillRoute();
}

void Route::ParseFrom(const Json::Array& data) {
    for (auto& item : data) {
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
    return busdb::Distance(route_.cbegin(), route_.cend(),
            [this](const string& from, const string& to) {return this->db_->Distance(from, to);});
}

DistanceType Route::LineDistance() const {
    return busdb::Distance(route_.cbegin(), route_.cend(),
            [this](const string& from, const string& to) {return this->db_->LineDistance(from, to);});
}

Json::Object Route::toJsonObject() const {
    auto distance = Distance();
    Json::Object json = {{"curvature", distance / LineDistance()},
            {"stop_count", (int)Stops().size()},
            {"unique_stop_count", (int)UniqueStops().size()}};

    if (distance - int(distance) > 0) json["route_length"] = distance;
    else json["route_length"] = int(distance);

    return json;

}

ostream& operator<<(ostream& out, const Route& r) {
    auto distance = r.Distance();
    return out << r.Stops().size() << " stops on route, " << r.UniqueStops().size()
            << " unique stops, " << int(distance) << " route length, "
            << distance / r.LineDistance() << " curvature";
}

class CircleRoute: public Route {
public:
    static string delimiter;

    virtual string_view Delimiter() const override;

    virtual void FillRoute() override {}
};

class TwoWayRoute: public Route {
public:
    static string delimiter;

    virtual string_view Delimiter() const override;

    virtual void FillRoute() override {
        if (route_.size() >= 2)
            copy(++route_.rbegin(), route_.rend(), back_inserter(route_));
    }
};
string CircleRoute::delimiter = " > ";

string TwoWayRoute::delimiter = " - ";

string_view CircleRoute::Delimiter() const {
    return CircleRoute::delimiter;;
}

string_view TwoWayRoute::Delimiter() const {
    return TwoWayRoute::delimiter;
}

shared_ptr<Route> Route::ParseRoute(string_view route_str) {
    shared_ptr<Route> route = nullptr;

    if (route_str.find(CircleRoute::delimiter) != string_view::npos) {
        route = make_shared<CircleRoute>();
    } else if (route_str.find(TwoWayRoute::delimiter) != string_view::npos) {
        route = make_shared<TwoWayRoute>();
    }

    if (route)
        route->ParseFrom(route_str);

    return route;
}

shared_ptr<Route> Route::ParseRoute(const Json::Object& data) {
    shared_ptr<Route> route = nullptr;

    if (data.at("is_roundtrip").AsBoolean()) {
        route = make_shared<CircleRoute>();
    } else {
        route = make_shared<TwoWayRoute>();
    }

    if (route) route->ParseFrom(data.at("stops").AsArray());

    return route;
}

}
