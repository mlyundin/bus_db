namespace {
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
            std::unordered_map <std::string_view, std::unordered_set<std::string_view>> adjusted_stops;
            for (const auto&[_, route]: buses_) {
                auto route_line = route->Stops();
                if (route_line.size() < 2) continue;

                auto it1 = std::begin(route_line);
                for (auto it2 = it1++; it1 != std::end(route_line); ++it2, ++it1) {
                    adjusted_stops[**it1].insert(**it2);
                    adjusted_stops[**it2].insert(**it1);
                }
            }

            return adjusted_stops;
        }

        auto SmoothStops() const {
            std::unordered_set <std::string_view> pivot_stops;
            {
                std::unordered_map<std::string_view, int> bus_count;
                std::unordered_map <std::string_view, Buses::mapped_type> first_bus;
                for (const auto&[_, route]: buses_) {
                    for (auto it: route->Stops()) {
                        const auto &c_stop = *it;
                        if (auto[pos, inserted] = first_bus.emplace(c_stop, route);
                                !inserted && pos->second != route)
                            bus_count[c_stop] += 10;

                        bus_count[c_stop] += 1;
                    }

                    for (auto item: route->EdgeStops())
                        pivot_stops.insert(*item);
                }

                for (const auto&[stop, _]: stops_) {
                    if (auto count = bus_count[stop]; count == 0 || count > 2)
                        pivot_stops.insert(stop);
                }
            }

            std::unordered_map <std::string_view, Svg::Point> res;
            for (const auto&[stop, point]: stops_) {
                if (pivot_stops.count(stop)) {
                    res[stop] = {point.longitude, point.latitude};
                }
            }

            for (const auto&[_, route]: buses_) {
                auto line_route = route->Stops();
                if (line_route.empty())
                    continue;

                auto it = std::begin(line_route), end = std::end(line_route);
                std::list to_smooth = {*it};
                for (++it; it != end; ++it) {
                    const auto &stop = **it;
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

        void ToSvgPoints() {
            if (not stops2points_.empty()) return;

            const auto stops = SmoothStops();
            if (stops.size() != stops_.size()) throw int(1);

            std::vector <std::pair<Svg::Point, std::string_view>> points;
            points.reserve(stops.size());
            std::transform(std::begin(stops), std::end(stops), std::back_inserter(points),
                           [](const auto &pair) { return std::make_pair(pair.second, pair.first); });

            static auto GetIndexes = [](auto &points, const auto &adjusted_stops, auto key) {
                std::sort(std::begin(points), std::end(points),
                          [key](auto &l, auto &r) { return key(l) < key(r); });

                const auto stop2pos = [&points]() {
                    std::unordered_map<std::string_view, int> res;
                    for (int i = 0; i < points.size(); ++i) {
                        res[points[i].second] = i;
                    }
                    return res;
                }();

                const auto n = points.size();
                std::vector<int> indexes(n, -1);
                for (size_t i = 0; i < n; ++i) {
                    auto[_, current_stop] = points[i];
                    int idx = -1;
                    if (auto it = adjusted_stops.find(current_stop); it != std::end(adjusted_stops)) {
                        for (auto adjusted_stop: it->second) {
                            idx = std::max(idx, indexes[stop2pos.at(adjusted_stop)]);
                        }
                    }

                    indexes[i] = idx + 1;
                }

                return indexes;
            };

            const auto adjusted_stops = GetAdjustedStops();
            {
                static auto key_x = [](auto &pair) -> double & { return pair.first.x; };
                const auto indexes = GetIndexes(points, adjusted_stops, key_x);

                const auto n = *std::max_element(std::begin(indexes), std::end(indexes));
                const auto x_step = n <= 0 ? decltype(render_settings_.width){} :
                                    (render_settings_.width - 2 * render_settings_.padding) / n;

                for (size_t i = 0; i < points.size(); ++i)
                    key_x(points[i]) = indexes[i] * x_step + render_settings_.padding;
            }

            {
                static auto key_y = [](auto &pair) -> double & { return pair.first.y; };
                const auto indexes = GetIndexes(points, adjusted_stops, key_y);

                const auto n = *std::max_element(std::begin(indexes), std::end(indexes));
                const auto y_step = n <= 0 ? decltype(render_settings_.width){} :
                                    (render_settings_.height - 2 * render_settings_.padding) / n;

                for (size_t i = 0; i < points.size(); ++i)
                    key_y(points[i]) = render_settings_.height - render_settings_.padding - indexes[i] * y_step;
            }

            for (auto[p, name]: points)
                stops2points_.insert({name, p});
        }

    public:
        using Stops = decltype(DataBase::stops_);
        using Buses = decltype(DataBase::buses_);

        Render(const Json::Object &s, const Stops &stops, const Buses &buses) : stops_(stops), buses_(buses) {
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
                    .layers = GetLayers(s.at("layers")),
                    .outer_margin = GetDouble(s.at("outer_margin"))
            };

            int i = 0;
            const auto n = render_settings_.color_palette.size();
            for (const auto& [name, _]: buses_) {
                buses2colors_[name] =  render_settings_.color_palette[(i++) % n];
            }

            ToSvgPoints();
        }

        Svg::Document BuildMap() const {
            if (render_settings_.layers.empty()) return {};

            Svg::Document map;
            for (const auto &layer: render_settings_.layers) {
                (this->*LAYER_ACTIONS.at(layer))(map);
            }

            return map;
        }

        void AddRect(Svg::Document& map) {
            // Rectangle
            auto outer_margin = render_settings_.outer_margin;
            auto rect = Svg::Rect{}.SetTLPoint({-outer_margin, -outer_margin})
                    .SetFillColor(render_settings_.underlayer_color)
                    .SetBRPoint({render_settings_.width + outer_margin, render_settings_.height + outer_margin});
            map.Add(std::move(rect));
        }

        void AddRoute(Svg::Document& map, const StopsRoute& route) const {
            std::vector<std::string> items;
            items.reserve(route.size());
            std::transform(std::begin(route), std::end(route), std::back_inserter(items),
                           [](const auto& item){return std::string(std::get<2>(item));});

            std::list<std::string_view> full_route;
            std::list<Svg::Polyline> polylines;
            {
                const std::string round_stroke = "round";
                for (int i = 1; i < items.size(); i += 2) {
                    const auto& bus_number = items[i];
                    std::string_view start_stop = items[i - 1], stop_stop = items[i + 1];

                    decltype(full_route) part;
                    const auto span_count = std::get<3>(route[i]);
                    const auto bus_route = buses_.at(bus_number);
                    for (auto it: bus_route->Stops()) {
                        const auto& current_stop = *it;
                        if (!part.empty()) part.push_back(current_stop);
                        if (current_stop == start_stop) {
                            part.clear();
                            part.push_back(current_stop);
                        } else if (current_stop == stop_stop) {

                            if (!part.empty() && part.size() == span_count + 1) {
                                Svg::Polyline polyline;
                                polyline.SetStrokeColor(buses2colors_.at(bus_number))
                                .SetStrokeWidth(render_settings_.line_width)
                                .SetStrokeLineCap(round_stroke)
                                .SetStrokeLineJoin(round_stroke);
                                for (auto stop: part) {
                                    polyline.AddPoint(stops2points_.at(stop));
                                }
                                polylines.push_back(std::move(polyline));

                                full_route.splice(std::end(full_route), std::move(part));
                                break;
                            }
                        }
                    }
                }
            }

            for (const auto& layer: render_settings_.layers) {
                if (layer == "bus_lines")
                {
                    // bus_lines
                    for (auto& poly: polylines) {
                        map.Add(poly);
                    }
                }
                else if (layer == "bus_labels")
                {
                    // bus_labels
                    for (int i = 1; i < items.size(); i += 2) {
                        const auto &bus_number = items[i], first_stop = items[i - 1],
                                last_stop = items[i + 1];
                        auto [bus_first_stop, bus_last_stop] = buses_.at(bus_number)->EdgeStops();

                        if (first_stop == *bus_first_stop || first_stop == *bus_last_stop) {
                            RenderBusLabel(map, bus_number, first_stop);
                        }

                        if (first_stop != last_stop && (last_stop == *bus_last_stop || last_stop == *bus_first_stop)) {
                            RenderBusLabel(map, bus_number, last_stop);
                        }
                    }
                }
                else if (layer == "stop_points")
                {
                    // stop_points
                    for (auto stop_name: full_route) {
                        map.Add(Svg::Circle{}.SetFillColor("white")
                                        .SetRadius(render_settings_.stop_radius).SetCenter(stops2points_.at(stop_name)));
                    }
                }
                else if (layer == "stop_labels")
                {
                    // stop_labels
                    for (int i = 0; i < items.size(); ++i) {
                        if (i % 2 != 0) continue;
                        RenderStopLabel(map, items[i]);
                    }
                }
            }
        }

    private:
        const Stops& stops_;
        const Buses& buses_;
        std::map<std::string_view, Svg::Point> stops2points_;
        std::unordered_map<std::string_view, Svg::Color> buses2colors_;
        static const std::unordered_map<std::string, void (Render::*)(Svg::Document &) const> LAYER_ACTIONS;

        struct RenderSettings {
            double width, height, padding, stop_radius, line_width;
            int stop_label_font_size;
            Svg::Point stop_label_offset;
            Svg::Color underlayer_color;
            double underlayer_width;
            std::vector <Svg::Color> color_palette;
            int bus_label_font_size;
            Svg::Point bus_label_offset;
            std::vector <std::string> layers;
            double outer_margin;
        };

        RenderSettings render_settings_;

        void RenderBusLines(Svg::Document &map) const {
            static const std::string round_stroke = "round";
            for (const auto&[name, route]: buses_) {
                Svg::Polyline polyline;

                polyline.SetStrokeColor(buses2colors_.at(name)).
                        SetStrokeWidth(render_settings_.line_width).SetStrokeLineCap(round_stroke).SetStrokeLineJoin(
                        round_stroke);

                for (auto stop: route->Stops()) {
                    polyline.AddPoint(stops2points_.at(*stop));
                }

                map.Add(std::move(polyline));
            }
        }

        void RenderBusLabel(Svg::Document &map, const std::string& bus_number, std::string_view stop_name) const {
            static const std::string round_stroke = "round";

            Svg::Text background;
            background.SetData(bus_number).SetFontFamily("Verdana").SetFontSize(render_settings_.bus_label_font_size).
                    SetFontWeight("bold").SetOffset(render_settings_.bus_label_offset).SetPoint(
                    stops2points_.at(stop_name));

            auto text = background;
            text.SetFillColor(buses2colors_.at(bus_number));

            background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                    SetStrokeWidth(render_settings_.underlayer_width).
                    SetStrokeColor(render_settings_.underlayer_color).SetFillColor(
                    render_settings_.underlayer_color);

            map.Add(std::move(background));
            map.Add(std::move(text));
        }

        void RenderBusLabels(Svg::Document &map) const {
            for (const auto&[name, route]: buses_) {
                const auto[first_stop, last_stop] = route->EdgeStops();

                RenderBusLabel(map, name, *first_stop);
                if (first_stop != last_stop) {
                    RenderBusLabel(map, name, *last_stop);
                }
            }
        }

        void RenderStopPoints(Svg::Document &map) const {
            for (auto[_, point]: stops2points_) {
                map.Add(Svg::Circle{}.SetFillColor("white").SetRadius(render_settings_.stop_radius).SetCenter(point));
            }
        }

        void RenderStopLabel(Svg::Document &map, const std::string& name) const {
            static const std::string round_stroke = "round";
            Svg::Text background;
            background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_.stop_label_font_size).
                    SetOffset(render_settings_.stop_label_offset).SetPoint(stops2points_.at(name));

            auto text = background;
            text.SetFillColor("black");

            background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                    SetStrokeWidth(render_settings_.underlayer_width).
                    SetStrokeColor(render_settings_.underlayer_color).SetFillColor(
                    render_settings_.underlayer_color);

            map.Add(std::move(background));
            map.Add(std::move(text));
        }

        void RenderStopLabels(Svg::Document &map) const {
            for (const auto&[name, _]: stops_) {
                RenderStopLabel(map, name);
            }
        }
    };

    const std::unordered_map<std::string, void (DataBase::Render::*)(
            Svg::Document &) const> DataBase::Render::LAYER_ACTIONS = {
            {"bus_lines",   &DataBase::Render::RenderBusLines},
            {"bus_labels",  &DataBase::Render::RenderBusLabels},
            {"stop_points", &DataBase::Render::RenderStopPoints},
            {"stop_labels", &DataBase::Render::RenderStopLabels},
    };

}