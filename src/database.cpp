#include <optional>
#include <memory>
#include <string>
#include <algorithm>
#include <tuple>

#include "database.h"
#include "route.h"
#include "code_profile.h"

using namespace Graph;

namespace {
    template<class Iter, class Key>
    std::array<Iter, 2> minmax_element_key(Iter begin, Iter end, Key key) {
        auto min_elem = begin, max_elem = begin;
        for(++begin; begin != end; ++begin) {
            if (key(*begin) < key(*min_elem)) min_elem = begin;
            if (key(*begin) > key(*max_elem)) max_elem = begin;
        }

        return {min_elem, max_elem};
    }

    template<class Iter>
    auto ToSvgPoints(const Iter begin, const Iter end, const busdb::DataBase::RenderSettings& settings) {
        std::list<busdb::Point> stops;
        std::transform(begin, end, std::back_inserter(stops), [](const auto& pair){
            return pair.second;
        });

        static auto is_equal = [](auto a, auto b){
            return std::abs(a - b) < 0.0001;
        };

        auto[min_lat_pos, max_lat_pos]  = minmax_element_key(std::begin(stops), std::end(stops),
                                                             [](auto point){return point.latitude;});
        auto[min_lon_pos, max_lon_pos]  = minmax_element_key(std::begin(stops), std::end(stops),
                                                             [](auto point){return point.longitude;});

        auto  min_lat = min_lat_pos->latitude, min_lon = min_lon_pos->longitude,
                max_lat = max_lat_pos->latitude, max_lon = max_lon_pos->longitude;

        auto diff_lat = max_lat - min_lat, diff_lon = max_lon - min_lon;

        using type_of_val = decltype(diff_lon);
        const auto zero_val = type_of_val{};
        auto padding = settings.padding;
        auto width_zoom_coef = is_equal(diff_lon, zero_val) ?
                               zero_val : (settings.width - 2 * padding) / diff_lon;
        auto height_zoom_coef = is_equal(diff_lat, zero_val) ?
                                zero_val : (settings.height - 2 * padding) / diff_lat;

        auto zoom_coef = is_equal(width_zoom_coef, zero_val) ||
                         (!is_equal(height_zoom_coef, zero_val) && height_zoom_coef < width_zoom_coef) ? height_zoom_coef : width_zoom_coef;

        std::map<std::string_view, Svg::Point> res;
        for (auto it = begin; it != end; ++it) {
            res.insert({it->first, {(it->second.longitude - min_lon) * zoom_coef + padding,
                                    (max_lat - it->second.latitude) * zoom_coef + padding}});
        }

        return res;
    }
}

namespace busdb {

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

std::tuple<double, std::list<DataBase::RouteItem>>
DataBase::GetRoute(const std::string& from, const std::string& to) const {
    if (!router_) return {-1, {}};
    if (from == to) return {0, {}};

    auto info = router_->BuildRoute(GetWaitStopVertexId(from),  GetWaitStopVertexId(to));
    if (!info) return {-1, {}};
    assert(info->edge_count >= 2);

    // first stops_.size() edges are wait bus edges
    auto start_edge_id = stops_.size();
    std::list<RouteItem> route;
    for (auto i = 0; i < info->edge_count; ++i) {
        auto edge_id = router_->GetRouteEdge(info->id, i);

        const auto& info = routes_->GetEdge(edge_id);

        if (i % 2 == 0) {
            route.push_back({RouteItemType::WAIT, info.weight,
                vertex_to_stop_[info.to], 0});
        } else {
            assert(edge_id >= start_edge_id);
            const auto& [bus_number, span_count] = edge_to_bus_[edge_id - start_edge_id];
            route.push_back({RouteItemType::BUS, info.weight, bus_number, span_count});
        }
    }

    router_->ReleaseRoute(info->id);
    return {info->weight, move(route)};
}

void DataBase::SetRouteSettings(const RouteSettings& settings) {
    this->route_settings_ = settings;
}

void DataBase::SetRenderSettings(RenderSettings&& settings) {
    this->render_settings_ = std::move(settings);
}

void DataBase::BuildRoutes() {
    if (!route_settings_) return;

    double bus_wait_time = route_settings_->bus_wait_time;
    double bus_velocity  = route_settings_->bus_velocity;

    auto stops_size = stops_.size();

    vertex_to_stop_.reserve(stops_size);
    vertex_to_stop_.clear();
    stop_to_vertex_.clear();
    edge_to_bus_.clear();

    routes_ = std::make_unique<DirectedWeightedGraph<double>>(stops_.size() * 2);
    Graph::VertexId current_vertex_id = {};
    for (const auto& [stop_name, temp]: stops_) {
        routes_->AddEdge({current_vertex_id + stops_size, current_vertex_id, bus_wait_time});

        vertex_to_stop_.push_back(stop_name);
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
        edge_to_bus_.emplace_back(edge_bus_number, edge_span_count);
    }

    router_ = std::make_unique<Router<double>>(*routes_);
}

Svg::Document DataBase::BuildMap() const {
    Svg::Document map;

    if (!render_settings_ || render_settings_->layers.empty()) return map;

    const auto stops2points = ToSvgPoints(std::begin(stops_), std::end(stops_), *render_settings_);
    const std::string round_stroke = "round";
    const auto n = render_settings_->color_palette.size();
    for (const auto& layer: render_settings_->layers) {
        if (layer == "bus_lines") {
            auto i = 0;
            for (const auto& [_, route]: buses_) {
                Svg::Polyline polyline;

                polyline.SetStrokeColor(render_settings_->color_palette[(i++) % n]).
                        SetStrokeWidth(render_settings_->line_width).SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke);

                for (auto stop: route->Stops()) {
                    polyline.AddPoint(stops2points.at(*stop));
                }

                map.Add(std::move(polyline));
            }
        }
        else if (layer == "bus_labels") {
            auto i = 0;
            for (const auto& [name, route]: buses_) {
                const auto& first_stop  = *route->FirstStop(), last_stop = *route->LastStop();

                Svg::Text background;
                background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_->bus_label_font_size).
                        SetFontWeight("bold").SetOffset(render_settings_->bus_label_offset).SetPoint(stops2points.at(first_stop));

                auto text = background;
                text.SetFillColor(render_settings_->color_palette[(i++) % n]);

                background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                        SetStrokeWidth(render_settings_->underlayer_width).
                        SetStrokeColor(render_settings_->underlayer_color).SetFillColor(render_settings_->underlayer_color);

                if (first_stop != last_stop) {
                    map.Add(background);
                    map.Add(text);

                    background.SetPoint(stops2points.at(last_stop));
                    text.SetPoint(stops2points.at(last_stop));
                }
                map.Add(std::move(background));
                map.Add(std::move(text));
            }
        }
        else if (layer == "stop_points") {
            for (auto [_, point]: stops2points) {
                map.Add(Svg::Circle{}.SetFillColor("white").SetRadius(render_settings_->stop_radius).SetCenter(point));
            }
        }
        else if (layer == "stop_labels") {
            for (const auto& [name, _]: stops_) {
                Svg::Text background;
                background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_->stop_label_font_size).
                        SetOffset(render_settings_->stop_label_offset).SetPoint(stops2points.at(name));

                auto text = background;
                text.SetFillColor("black");

                background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                        SetStrokeWidth(render_settings_->underlayer_width).
                        SetStrokeColor(render_settings_->underlayer_color).SetFillColor(render_settings_->underlayer_color);

                map.Add(std::move(background));
                map.Add(std::move(text));
            }
        }
    }

    return map;
}

}
