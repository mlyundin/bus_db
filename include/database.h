#pragma once

#include<set>
#include<unordered_map>
#include<string>
#include<string_view>
#include<optional>
#include<memory>
#include<tuple>
#include<list>
#include<map>

#include "common.h"
#include "graph.h"
#include "router.h"
#include "svg.h"

namespace busdb {

class Route;

class DataBase {
public:
    struct RouteSettings {
        int bus_wait_time;
        int bus_velocity;
    };

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

    enum class RouteItemType {
        WAIT = 0,
        BUS
    };

    using RouteItem = std::tuple<DataBase::RouteItemType, double, std::string_view, int>;

    DataBase() = default;

    DistanceType Distance(const std::string& stop1,
            const std::string& stop2) const;

    DistanceType LineDistance(const std::string& stop1,
            const std::string& stop2) const;

    void AddStop(std::string stop, Point location,
            std::list<std::pair<std::string, int>> distances);

    void AddBus(std::string number, std::shared_ptr<Route> route);

    std::shared_ptr<Route> GetBusRoute(const std::string& number) const;

    std::optional<std::set<std::string_view>> GetStopBuses(
            std::string_view stop) const;

    std::tuple<double, std::list<RouteItem>>
    GetRoute(const std::string& from, const std::string& to) const;

    void SetRouteSettings(const RouteSettings& settings);
    void SetRenderSettings(RenderSettings&& settings);

    void BuildRoutes();

    Svg::Document BuildMap() const;

private:
    std::map<std::string, Point> stops_;
    std::map<std::string, std::shared_ptr<Route>> buses_;
    std::unordered_map<std::string_view, std::set<std::string_view>> stop_buses_;
    mutable std::unordered_map<std::string_view,
            std::unordered_map<std::string_view, DistanceType>> distance_hash_;

    std::optional<RouteSettings> route_settings_ = std::nullopt;
    std::optional<RenderSettings> render_settings_ = std::nullopt;

    std::unique_ptr<Graph::DirectedWeightedGraph<double>> routes_ = nullptr;
    std::unique_ptr<Graph::Router<double>> router_ = nullptr;

    std::vector<std::string_view> vertex_to_stop_;
    std::unordered_map<std::string_view, Graph::VertexId> stop_to_vertex_;

    std::vector<std::pair<std::string_view, int>> edge_to_bus_;

    Graph::VertexId GetWaitStopVertexId(std::string_view stop) const;
};

}
