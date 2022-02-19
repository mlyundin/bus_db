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

        void ToSvgPoints() const {
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
                    .layers = GetLayers(s.at("layers"))
            };
        }

        Svg::Document BuildMap() const {
            if (render_settings_.layers.empty()) return {};

            ToSvgPoints();
            Svg::Document map;
            for (const auto &layer: render_settings_.layers) {
                (this->*LAYER_ACTIONS.at(layer))(map);
            }

            return map;
        }

    private:
        const Stops &stops_;
        const Buses &buses_;
        mutable std::map <std::string_view, Svg::Point> stops2points_;
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
        };

        RenderSettings render_settings_;

        void RenderBusLines(Svg::Document &map) const {
            const std::string round_stroke = "round";
            const auto n = render_settings_.color_palette.size();
            auto i = 0;
            for (const auto&[_, route]: buses_) {
                Svg::Polyline polyline;

                polyline.SetStrokeColor(render_settings_.color_palette[(i++) % n]).
                        SetStrokeWidth(render_settings_.line_width).SetStrokeLineCap(round_stroke).SetStrokeLineJoin(
                        round_stroke);

                for (auto stop: route->Stops()) {
                    polyline.AddPoint(stops2points_.at(*stop));
                }

                map.Add(std::move(polyline));
            }
        }

        void RenderBusLabels(Svg::Document &map) const {
            const std::string round_stroke = "round";
            const auto n = render_settings_.color_palette.size();
            auto i = 0;
            for (const auto&[name, route]: buses_) {
                const auto[first_stop, last_stop] = route->EdgeStops();

                Svg::Text background;
                background.SetData(name).SetFontFamily("Verdana").SetFontSize(render_settings_.bus_label_font_size).
                        SetFontWeight("bold").SetOffset(render_settings_.bus_label_offset).SetPoint(
                        stops2points_.at(*first_stop));

                auto text = background;
                text.SetFillColor(render_settings_.color_palette[(i++) % n]);

                background.SetStrokeLineCap(round_stroke).SetStrokeLineJoin(round_stroke).
                        SetStrokeWidth(render_settings_.underlayer_width).
                        SetStrokeColor(render_settings_.underlayer_color).SetFillColor(
                        render_settings_.underlayer_color);

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

        void RenderStopPoints(Svg::Document &map) const {
            for (auto[_, point]: stops2points_) {
                map.Add(Svg::Circle{}.SetFillColor("white").SetRadius(render_settings_.stop_radius).SetCenter(point));
            }
        }

        void RenderStopLabels(Svg::Document &map) const {
            const std::string round_stroke = "round";
            for (const auto&[name, _]: stops_) {
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