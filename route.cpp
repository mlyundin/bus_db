#include "route.h"
#include "database.h"

using namespace std;

namespace busdb {

void Route::ParseFrom(string_view stops) {
    auto delimiter = Delimiter();
    while (stops.size()) {
        auto [it, inserted] = stops_.insert(string(ReadToken(stops, delimiter)));
        route_.push_back(it);
    }
}

void Route::ParseFrom(const Json::Array& data) {
    for (auto& item : data) {
        auto [it, inserted] = stops_.insert(item.AsString());
        route_.push_back(it);
    }
}

Route::StopsContainer::iterator Route::begin() {
    return stops_.begin();
}
Route::StopsContainer::iterator Route::end() {
    return stops_.end();
}

Route::StopsContainer::const_iterator Route::begin() const {
    return stops_.begin();
}
Route::StopsContainer::const_iterator Route::end() const {
    return stops_.end();
}

void Route::SetDB(const DataBase* db) {
    db_ = db;
}

int Route::UniqueStops() const {
    return stops_.size();
}

DistanceType Route::Distance() const {
    return Distance(route_.cbegin(), route_.cend(),
            [this](const string& from, const string& to) {return this->db_->Distance(from, to);});
}

DistanceType Route::LineDistance() const {
    return Distance(route_.cbegin(), route_.cend(),
            [this](const string& from, const string& to) {return this->db_->LineDistance(from, to);});
}

Json::Object Route::toJsonObject() const {
    auto distance = Distance();
    return { {"route_length", distance}, {"curvature", distance / LineDistance()},
        {   "stop_count", Stops()}, {"unique_stop_count", UniqueStops()}};

}

ostream& operator<<(ostream& out, const Route& r) {
    auto distance = r.Distance();
    return out << r.Stops() << " stops on route, " << r.UniqueStops()
            << " unique stops, " << int(distance) << " route length, "
            << distance / r.LineDistance() << " curvature";
}

class CircleRoute: public Route {
public:
    static string delimiter;

    virtual string_view Delimiter() const override;

    int Stops() const override;
};

class TwoWayRoute: public Route {
public:
    static string delimiter;

    virtual string_view Delimiter() const override;

    int Stops() const override;

    DistanceType Distance() const override;

    DistanceType LineDistance() const override;
};
string CircleRoute::delimiter = " > ";

string TwoWayRoute::delimiter = " - ";

string_view CircleRoute::Delimiter() const {
    return CircleRoute::delimiter;;
}

int CircleRoute::Stops() const {
    return route_.size();
}

string_view TwoWayRoute::Delimiter() const {
    return TwoWayRoute::delimiter;
}

int TwoWayRoute::Stops() const {
    return route_.size() * 2 - 1;
}

DistanceType TwoWayRoute::Distance() const {
    auto distance =
            [this](const string& from, const string& to) {return this->db_->Distance(from, to);};
    return Route::Distance(route_.cbegin(), route_.cend(), distance)
            + Route::Distance(route_.crbegin(), route_.crend(), distance);
}

DistanceType TwoWayRoute::LineDistance() const {
    return Route::LineDistance() * 2;
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

shared_ptr<Route> Route::ParseRoute(const Json::Array& data) {
    shared_ptr<Route> route = nullptr;
    auto size = data.size();

    if (size && data[0].AsString() == data[size - 1].AsString()) {
        route = make_shared<CircleRoute>();
    } else {
        route = make_shared<TwoWayRoute>();
    }

    if (route)
        route->ParseFrom(data);

    return route;
}

}
