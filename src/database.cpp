#include <optional>
#include <memory>
#include <string>
#include <algorithm>
#include <tuple>

#include "database.h"
#include "route.h"
#include "render_impl.h"

using namespace Graph;

namespace busdb {

DataBase::DataBase(){

}

DataBase::~DataBase(){

}

DistanceType DataBase::LineDistance(const std::string& stop1, const std::string& stop2) const {
    return busdb::Distance(stops_.at(stop1), stops_.at(stop2));
}

DistanceType DataBase::Distance(const std::string& stop1, const std::string& stop2) const {
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

void DataBase::AddStop(std::string stop, Point location,
                       std::list<std::pair<std::string, int>> distances) {
    auto [it, inserted] = stops_.insert({move(stop), location});
    if(!inserted) it->second = location;

    std::string_view stop_name = it->first;
    if (stop_buses_.count(stop_name) == 0) {
        stop_buses_[stop_name];
    }

    for(auto& [another_stop_name, distance]: distances) {
        auto [another_it, temp] = stops_.insert( {move(another_stop_name), {}});
        distance_hash_[stop_name][another_it->first] = distance;

        distance_hash_[another_it->first].insert( {stop_name, distance});
    }
}

void DataBase::AddBus(std::string number, std::shared_ptr<Route> route) {
    route->SetDB(this);
    auto [it, inserted] = buses_.insert( {move(number), move(route)});

    if (inserted) {
        std::string_view bus_number = it->first;
        for (const auto& stop : it->second->UniqueStops())
            stop_buses_[stop].insert(bus_number);
    }
}

std::shared_ptr<Route> DataBase::GetBusRoute(const std::string& number) const {
    if (auto it = buses_.find(number); it != buses_.end())
        return it->second;

    return nullptr;
}

std::optional<std::set<std::string_view>> DataBase::GetStopBuses(std::string_view stop) const {
    if (auto it = stop_buses_.find(stop); it != stop_buses_.end())
        return it->second;

    return std::nullopt;
}
Graph::VertexId DataBase::GetWaitStopVertexId(std::string_view stop) const{
    if(auto it = stop_to_vertex_.find(stop); it != stop_to_vertex_.end())
         return it->second + stops_.size();

     return -1;
}


std::tuple<double, DataBase::StopsRoute, Svg::Document>
DataBase::GetRoute(const std::string& from, const std::string& to) const {
    if (!router_) return {-1, {}, {}};

    auto map = BuildMap();
    if (render_) render_->AddRect(map);

    if (from == to) return {0, {}, std::move(map)};

    auto info = router_->BuildRoute(GetWaitStopVertexId(from),  GetWaitStopVertexId(to));
    if (!info) return {-1, {}, {}};
    assert(info->edge_count >= 2);

    // first stops_.size() edges are wait bus edges
    const auto start_edge_id = stops_.size();
    StopsRoute route;
    for (auto i = 0; i < info->edge_count; ++i) {
        auto edge_id = router_->GetRouteEdge(info->id, i);
        const auto& info = routes_->GetEdge(edge_id);

        if (i % 2 == 0) {
            route.emplace_back(RouteItemType::WAIT, info.weight,
                               vertex2stop_[info.to], 0);
        } else {
            assert(edge_id >= start_edge_id);
            const auto& [bus_number, span_count] = edge2bus_[edge_id - start_edge_id];
            route.emplace_back(RouteItemType::BUS, info.weight, bus_number, span_count);
        }
    }

    if (render_) {
        route.emplace_back(RouteItemType::WAIT, 0, to, 0);
        render_->AddRoute(map, route);
        route.pop_back();
    }

    router_->ReleaseRoute(info->id);
    return {info->weight, std::move(route), std::move(map)};
}

void DataBase::SetRouteSettings(const Json::Object& in_data) {
    if (in_data.count("routing_settings")) {
        const auto &s = in_data.at("routing_settings").AsObject();
        route_settings_ = {.bus_wait_time=s.at("bus_wait_time").AsInt(),
                           .bus_velocity=s.at("bus_velocity").AsInt()};
    }
}

void DataBase::SetRenderSettings(const Json::Object& in_data) {
    if (in_data.count("render_settings") == 0 || render_)
        return;

    render_ = std::make_unique<Render>(in_data.at("render_settings").AsObject(), stops_, buses_);
}

void DataBase::BuildRoutes() {
    if (!route_settings_) return;

    double bus_wait_time = route_settings_->bus_wait_time;
    double bus_velocity  = route_settings_->bus_velocity;

    auto stops_size = stops_.size();

    vertex2stop_.reserve(stops_size);
    vertex2stop_.clear();
    stop_to_vertex_.clear();
    edge2bus_.clear();

    routes_ = std::make_unique<DirectedWeightedGraph<double>>(stops_.size() * 2);
    Graph::VertexId current_vertex_id = {};
    for (const auto& [stop_name, temp]: stops_) {
        routes_->AddEdge({current_vertex_id + stops_size, current_vertex_id, bus_wait_time});

        vertex2stop_.push_back(stop_name);
        stop_to_vertex_.insert({stop_name, current_vertex_id++});
    }

    std::vector<std::tuple<std::string_view, int, Edge<double>>> edge_hash(stops_size * stops_size,
            {{}, {}, {}});
    for(const auto& [bus_number, route]: buses_) {
        const auto& stops = route->Stops();
        if (stops.size() < 2) continue;

        auto end = --stops.end();
        for(auto it_from = stops.begin(); it_from != end; ++it_from) {
            auto v_from = stop_to_vertex_[*(*it_from)];
            auto span_count = 0;
            double distance = 0;

            auto begin = it_from;
            for(auto it_to = ++begin, it_prev = it_from; it_to != stops.end(); ++it_to, ++it_prev) {
                distance += Distance(*(*it_prev), *(*it_to));
                span_count += 1;
                double time = distance / bus_velocity / 1000 * 60;

                auto v_to = stop_to_vertex_[*(*it_to)];
                auto edge_hash_idx = v_from * stops_size + v_to;
                auto& [edge_bus_number, edge_span_count, edge] = edge_hash[edge_hash_idx];
                if (edge_span_count == 0 || edge.weight > time) {
                    edge = {v_from, v_to + stops_size, time};
                    edge_bus_number = bus_number;
                    edge_span_count = span_count;
                }
            }
        }
    }

    for (const auto& [edge_bus_number, edge_span_count, edge]: edge_hash) {
        if (edge_span_count == 0) continue;
        routes_->AddEdge(edge);
        edge2bus_.emplace_back(edge_bus_number, edge_span_count);
    }

    router_ = std::make_unique<Router<double>>(*routes_);
}

Svg::Document DataBase::BuildMap() const {
    return render_ ? render_->BuildMap() : Svg::Document();
}

}
