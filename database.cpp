#include <optional>
#include <memory>
#include <string>
#include <algorithm>

#include "database.h"
#include "route.h"

using namespace std;
using namespace Graph;

namespace busdb {

DistanceType DataBase::LineDistance(const string& stop1,
        const string& stop2) const {
    return busdb::Distance(stops_.at(stop1), stops_.at(stop2));
}

DistanceType DataBase::Distance(const string& stop1,
        const string& stop2) const {
    if (auto it1 = distance_hash_.find(stop1); it1 != distance_hash_.end()) {
        if (auto it2 = it1->second.find(stop2); it2 != it1->second.end()) {
            return it2->second;
        }
    }

    DistanceType res = { };
    auto it_s1 = stops_.find(stop1), it_s2 = stops_.find(stop2);
    if (it_s1 != stops_.end() && it_s2 != stops_.end()) {
        res = distance_hash_[it_s1->first][it_s2->first] = LineDistance(stop1,
                stop2);
    }

    return res;
}

void DataBase::AddStop(string stop, Point location,
        list<pair<string, int>> distances) {
    auto [it, inserted] = stops_.insert({move(stop), location});
    if(!inserted) it->second = location;

    string_view stop_name = it->first;
    if (stop_buses_.count(stop_name) == 0) {
        stop_buses_[stop_name];
    }

    for(auto& [another_stop_name, distance]: distances) {
        auto [another_it, temp] = stops_.insert( {move(another_stop_name), {}});
        distance_hash_[stop_name][another_it->first] = distance;

        distance_hash_[another_it->first].insert( {stop_name, distance});
    }
}

void DataBase::AddBus(string number, shared_ptr<Route> route) {
    route->SetDB(this);
    auto [it, inserted] = buses_.insert( {move(number), move(route)});

    if (inserted) {
        string_view bus_number = it->first;
        for (const auto& stop : it->second->UniqueStops())
            stop_buses_[stop].insert(bus_number);
    }
}

shared_ptr<Route> DataBase::GetBusRoute(const string& number) const {
    shared_ptr<Route> res = nullptr;
    if (auto it = buses_.find(number); it != buses_.end())
        res = it->second;

    return res;
}

optional<set<string_view>> DataBase::GetStopBuses(string_view stop) const {
    auto it = stop_buses_.find(stop);
    if (it == stop_buses_.end())
        return nullopt;

    return it->second;
}

tuple<double, list<DataBase::RouteItem>>
DataBase::GetRoute(const string& from, const string& to) const {
    if (!router_) return {-1, {}};

    VertexId v_from = stop_to_vertex_.at(from),
            v_to = stop_to_vertex_.at(to);
    auto info = router_->BuildRoute(v_from, v_to);

    if (!info) return {-1, {}};

    list<RouteItem> route;
    RouteItem current_item = {RouteItemType::WAIT, settings->bus_wait_time, from, 0};
    for (auto i = 0; i < info->edge_count; ++i) {
        auto edge_id = router_->GetRouteEdge(info->id, i);
        const auto& info = routes_->GetEdge(edge_id);
        if(auto it = edge_to_bus_.find(edge_id); it != edge_to_bus_.end()) {
            if(get<0>(current_item) == RouteItemType::WAIT) {
                route.push_back(current_item);
                current_item = {RouteItemType::BUS, info.weight, it->second, 1};
            }
            else {
                get<1>(current_item) += info.weight;
                get<3>(current_item) += 1;
            }
        } else if (info.to < vertex_to_stop_.size()) {
            route.push_back(current_item);
            current_item = {RouteItemType::WAIT, settings->bus_wait_time,
                    vertex_to_stop_[info.to], 0};
        }
    }

    router_->ReleaseRoute(info->id);
    return {info->weight, move(route)};
}

void DataBase::SetSettings(const Settings& settings) {
    this->settings = settings;
}

void DataBase::BuildRoutes() {
    if (!settings) return;

    double bus_wait_time = settings->bus_wait_time;
    double bus_velocity  = settings->bus_velocity;

    auto stops_size = stops_.size();

    Graph::VertexId current_vertex_id = {};
    vertex_to_stop_.reserve(stops_size);
    vertex_to_stop_.clear();
    stop_to_vertex_.clear();
    edge_to_bus_.clear();

    for (const auto& [stop_name, temp]: stops_) {
        vertex_to_stop_.push_back(stop_name);
        stop_to_vertex_.insert({stop_name, current_vertex_id++});
    }

    auto vertex_count = stops_size;
    for(const auto& [temp, route]: buses_) vertex_count += route->Stops().size();
    routes_ = make_unique<DirectedWeightedGraph<double>>(vertex_count);
    for(const auto& [bus_number, route]: buses_) {
        const auto& stops = route->Stops();
        auto prev_stop_it = stops.end();
        for(auto it = stops.begin(); it != stops.end(); ++it) {
            Graph::VertexId current_stop_id = current_vertex_id++;
            const auto& stop_name = *(*it);

            auto wait_stop_id = stop_to_vertex_.at(stop_name);
            routes_->AddEdge({wait_stop_id, current_stop_id, bus_wait_time});
            routes_->AddEdge({current_stop_id, wait_stop_id, 0.0});

            if (prev_stop_it != stops.end()) {
                // covert to meters per min
                auto time = Distance(*(*prev_stop_it), stop_name) / bus_velocity / 1000 * 60;
                auto edge_id = routes_->AddEdge({current_stop_id - 1, current_stop_id, time});
                edge_to_bus_.insert({edge_id, bus_number});
            }

            prev_stop_it = it;
        }
    }
    router_ = make_unique<Router<double>>(*routes_);
}

}
