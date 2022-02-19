#include <optional>
#include <memory>
#include <string>
#include <algorithm>
#include <tuple>
#include <cmath>

#include "database.h"
#include "route.h"
#include "code_profile.h"

using namespace Graph;

namespace {
    template<class Iter, class Key>
    std::array<Iter, 2> MinmaxElementKey(Iter begin, Iter end, Key key) {
        auto min_elem = begin, max_elem = begin;
        for(++begin; begin != end; ++begin) {
            if (key(*begin) < key(*min_elem)) min_elem = begin;
            if (key(*begin) > key(*max_elem)) max_elem = begin;
        }

        return {min_elem, max_elem};
    }

    using namespace Json;
    double GetDouble(const Node& node) {
        return (Node::Type)node.index() == Node::Type::DoubleType ? node.AsDouble() : node.AsInt();
    }

    Svg::Color GetColor(const Node& node) {
        if (auto type = (Node::Type)node.index();type == Node::Type::ArrayType) {
            const auto& arr = node.AsArray();
            if (arr.size() == 3) {
                return {Svg::Rgb{arr[0].AsInt(), arr[1].AsInt(), arr[2].AsInt()}};
            }
            else {
                return {Svg::Rgba{arr[0].AsInt(), arr[1].AsInt(), arr[2].AsInt(), arr[3].AsDouble()}};
            }
        }
        else if (type == Node::Type::StringType) {
            return {node.AsString()};
        }

        return Svg::NoneColor;
    }

    Svg::Point GetPoint(const Node& node) {
        const auto& arr = node.AsArray();
        return {GetDouble(arr[0]), GetDouble(arr[1])};
    }

    auto GetLayers(const Node& node) {
        const auto& arr = node.AsArray();
        std::vector<std::string> res;
        res.reserve(arr.size());
        std::transform(std::begin(arr), std::end(arr), std::back_inserter(res), [](const auto& item){
            return item.AsString();
        });

        return res;
    }

    auto GetPallet(const Node& node) {
        std::vector<Svg::Color> color_palette;
        const auto& arr = node.AsArray();
        color_palette.reserve(arr.size());
        std::transform(std::begin(arr), std::end(arr), std::back_inserter(color_palette), GetColor);
        return color_palette;
    };
}

namespace busdb {

class DataBase::Render {
    auto GetAdjustedStops() const {
        std::unordered_map<std::string_view, std::unordered_set<std::string_view>> adjasted_stops;
        for (const auto& [_, route]: buses_) {
            auto route_line = route->Stops();
            if (route_line.size() <= 1) continue;
            auto it1 = std::begin(route_line);

            for (auto it2 = it1++; it1 != std::end(route_line); ++it2, ++it1) {
                adjasted_stops[*(*it1)].insert(*(*it2));
                adjasted_stops[*(*it2)].insert(*(*it1));
            }
        }

        return adjasted_stops;
    }

    auto SmoothStops() const {
        std::unordered_set<std::string_view> pivot_stops;
        {
            std::unordered_map<std::string_view, int> bus_count;
            std::unordered_map<std::string_view, Buses::mapped_type> first_bus;
            for (const auto& [_, route]: buses_) {
                for (auto it: route->Stops()) {
                    const auto& c_stop = *it;
                    if (auto [pos, inserted] = first_bus.emplace(c_stop, route);
                    !inserted && pos->second != route) bus_count[c_stop] += 10;

                    bus_count[c_stop] += 1;
                }

                for (auto item: route->EdgeStops())
                    pivot_stops.insert(*item);
            }

            for (const auto& [stop, _]: stops_) {
                if (auto count = bus_count[stop]; count == 0 || count > 2)
                    pivot_stops.insert(stop);
            }
        }

        std::unordered_map<std::string_view, Svg::Point> res;
        for (const auto& [stop, point]: stops_) {
            if (pivot_stops.count(stop)) {
                res[stop] = {point.longitude, point.latitude};
            }
        }

        for (const auto& [_, route]: buses_) {
            auto line_route = route->Stops();
            if (line_route.empty())
                continue;

            auto it = std::begin(line_route), end = std::end(line_route);
            std::list to_smooth = {*it};
            for (++it; it != end; ++it) {
                const std::string& stop = *(*it);
                if (pivot_stops.count(stop) == 0) {
                    to_smooth.push_back(*it);
                    continue;
                }

                if (const auto n = to_smooth.size(); n > 1) {
                    const auto ps = stops_.at(*to_smooth.front()), pe = stops_.at(stop);
                    const double lon_step = (pe.longitude - ps.longitude) / n;
                    const double lat_step = (pe.latitude - ps.latitude) / n;

                    auto pos = std::begin(to_smooth);
                    for (int i = 1; i < n; ++i) {
                        res[**(++pos)] = {ps.longitude + lon_step * i, ps.latitude + lat_step * i};
                    }
                }

                to_smooth = {*it};
            }
        }

        return res;
    }

    void ToSvgPoints() const {
        if (not stops2points_.empty()) return;

        auto stops = SmoothStops();

        if (stops.size() != stops_.size()) throw int(1);
        std::vector<std::pair<Svg::Point, std::string_view>> points;
        points.reserve(stops.size());
        for (auto it = std::cbegin(stops); it != std::cend(stops); ++it)
            points.emplace_back(it->second, it->first);

        static auto GetIndexes = [](auto& points, const auto& adjusted_stops, auto key){
            std::sort(std::begin(points), std::end(points),
                      [key](auto& l, auto& r){return key(l) < key(r);});

            std::vector<int> indexes(points.size(), 0);
            int idx = 0;
            std::list<std::string_view> merged_stops = {points[0].second};
            for (size_t i = 1; i < points.size(); ++i) {
                auto [_, current_stop] = points[i];
                for (auto stop: merged_stops) {
                    if (auto it = adjusted_stops.find(stop); it != std::end(adjusted_stops) &&
                    it->second.count(current_stop) != 0) {
                        idx += 1;
                        merged_stops.clear();
                        break;
                    }
                }
                indexes[i] = idx;
                merged_stops.push_back(current_stop);
            }

            return indexes;
        };

        auto adjusted_stops = GetAdjustedStops();
        {
            static auto key_x = [](auto& pair)->double&{return pair.first.x;};
            auto indexes = GetIndexes(points, adjusted_stops, key_x);

            const auto n = indexes.back();
            auto x_step = n <= 0 ? decltype(render_settings_.width){} :
                          (render_settings_.width - 2 * render_settings_.padding) / n;

            for (size_t i = 0; i < points.size(); ++i)
                key_x(points[i]) = indexes[i] * x_step + render_settings_.padding;
        }

        {
            static auto key_y = [](auto& pair)->double&{return pair.first.y;};
            auto indexes = GetIndexes(points, adjusted_stops, key_y);

            const auto n = indexes.back();
            auto y_step = n <= 0 ? decltype(render_settings_.width){} :
                          (render_settings_.height - 2 * render_settings_.padding) / n;

            for (size_t i = 0; i < points.size(); ++i)
                key_y(points[i]) = render_settings_.height - render_settings_.padding - indexes[i] * y_step;
        }

        for(auto [p, name]: points)
            stops2points_.insert({name, p});
    }

public:
    using Stops = decltype(DataBase::stops_);
    using Buses = decltype(DataBase::buses_);

    Render(const Json::Object& s, const Stops& stops, const Buses& buses): stops_(stops), buses_(buses) {


        render_settings_ = {.width = GetDouble(s.at("width")),
                .height = GetDouble(s.at("height")),
                .padding = GetDouble(s.at("padding")),
                .stop_radius = GetDouble(s.at("stop_radius")),
                .line_width = GetDouble(s.at("line_width")),
                .stop_label_font_size = s.at("stop_label_font_size").AsInt(),
                .stop_label_offset = GetPoint(s.at("stop_label_offset")),
                .underlayer_color = GetColor(s.at("underlayer_color")),
                .underlayer_width = GetDouble(s.at("underlayer_width")),
                .color_palette = GetPallet(s.at("color_palette")),
                .bus_label_font_size = s.at("bus_label_font_size").AsInt(),
                .bus_label_offset = GetPoint(s.at("bus_label_offset")),
                .layers = GetLayers(s.at("layers"))
        };
    }

    Svg::Document BuildMap() const {
        if (render_settings_.layers.empty()) return {};

        ToSvgPoints();
        Svg::Document map;
        for (const auto& layer: render_settings_.layers) {
            (this->*LAYER_ACTIONS.at(layer))(map);
        }

        return map;
    }

private:
    const Stops& stops_;
    const Buses& buses_;
    mutable std::map<std::string_view, Svg::Point> stops2points_;
    static const std::unordered_map<std::string, void (Render::*)(Svg::Document&) const> LAYER_ACTIONS;

    struct RenderSettings {
        double width, height, padding, stop_radius, line_width;
        int stop_label_font_size;
        Svg::Point stop_label_offset;
        Svg::Color underlayer_color;
        double underlayer_width;
        std::vector<Svg::Color> color_palette;
        int bus_label_font_size;
        Svg::Point bus_label_offset;
        std::vector<std::string> layers;
    };

    RenderSettings render_settings_;

    void RenderBusLines(Svg::Document& map) const {
        const std::string round_stroke = "round";
        const auto n = render_settings_.color_palette.size();
        auto i = 0;
        for (const auto& [_, route]: buses_) {
            Svg::Polyline polyline;

            polyline.SetStrokeColor(render_settings_.color_palette[(i++) % n]).
                    SetStrokeWidth(render_settings_.line_width).SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke);

            for (auto stop: route->Stops()) {
                polyline.AddPoint(stops2points_.at(*stop));
            }

            map.Add(std::move(polyline));
        }
    }

    void RenderBusLabels(Svg::Document& map) const {
        const std::string round_stroke = "round";
        const auto n = render_settings_.color_palette.size();
        auto i = 0;
        for (const auto& [name, route]: buses_) {
            const auto [first_stop, last_stop] = route->EdgeStops();

            Svg::Text background;
            background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_.bus_label_font_size).
                    SetFontWeight("bold").SetOffset(render_settings_.bus_label_offset).SetPoint(stops2points_.at(*first_stop));

            auto text = background;
            text.SetFillColor(render_settings_.color_palette[(i++) % n]);

            background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                    SetStrokeWidth(render_settings_.underlayer_width).
                    SetStrokeColor(render_settings_.underlayer_color).SetFillColor(render_settings_.underlayer_color);

            if (first_stop != last_stop) {
                map.Add(background);
                map.Add(text);

                background.SetPoint(stops2points_.at(*last_stop));
                text.SetPoint(stops2points_.at(*last_stop));
            }
            map.Add(std::move(background));
            map.Add(std::move(text));
        }
    }

    void RenderStopPoints(Svg::Document& map) const {
        for (auto [_, point]: stops2points_) {
            map.Add(Svg::Circle{}.SetFillColor("white").SetRadius(render_settings_.stop_radius).SetCenter(point));
        }
    }

    void RenderStopLabels(Svg::Document& map) const {
        const std::string round_stroke = "round";
        for (const auto& [name, _]: stops_) {
            Svg::Text background;
            background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_.stop_label_font_size).
                    SetOffset(render_settings_.stop_label_offset).SetPoint(stops2points_.at(name));

            auto text = background;
            text.SetFillColor("black");

            background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                    SetStrokeWidth(render_settings_.underlayer_width).
                    SetStrokeColor(render_settings_.underlayer_color).SetFillColor(render_settings_.underlayer_color);

            map.Add(std::move(background));
            map.Add(std::move(text));
        }
    }
};

const std::unordered_map<std::string, void (DataBase::Render::*)(Svg::Document&) const> DataBase::Render::LAYER_ACTIONS = {
        {"bus_lines",   &DataBase::Render::RenderBusLines},
        {"bus_labels",  &DataBase::Render::RenderBusLabels},
        {"stop_points", &DataBase::Render::RenderStopPoints},
        {"stop_labels", &DataBase::Render::RenderStopLabels},
};

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
    return render_ ? render_->BuildMap() : Svg::Document();
}

}
