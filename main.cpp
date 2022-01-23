#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#include "common.h"
#include "database.h"
#include "request.h"
#include "json.h"
#include "code_profile.h"

using namespace busdb;
using namespace Json;

namespace {

    double getDouble(const Node& node) {
        return (Node::Type)node.index() == Node::Type::DoubleType ? node.AsDouble() : node.AsInt();
    }

    Svg::Color getColor(const Node& node) {
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

    Svg::Point getPoint(const Node& node) {
        const auto& arr = node.AsArray();
        return {getDouble(arr[0]), getDouble(arr[1])};
    }

    void ReadSettings(const Object &in_data, DataBase &db) {
        if (in_data.count("routing_settings")) {
            const auto &s = in_data.at("routing_settings").AsObject();
            db.SetRouteSettings({.bus_wait_time=s.at("bus_wait_time").AsInt(),
                                        .bus_velocity=s.at("bus_velocity").AsInt()});
        }

        if (in_data.count("render_settings")) {
            const auto &s = in_data.at("render_settings").AsObject();
            static auto getPallet = [](const auto& node) {
                std::vector<Svg::Color> color_palette;
                const auto& arr = node.AsArray();
                color_palette.reserve(arr.size());
                std::transform(std::begin(arr), std::end(arr), std::back_inserter(color_palette), getColor);
                return color_palette;
            };

            db.SetRenderSettings({
                .width = getDouble(s.at("width")),
                .height = getDouble(s.at("height")),
                .padding = getDouble(s.at("padding")),
                .stop_radius = getDouble(s.at("stop_radius")),
                .line_width = getDouble(s.at("line_width")),
                .stop_label_font_size = s.at("stop_label_font_size").AsInt(),
                .stop_label_offset = getPoint(s.at("stop_label_offset")),
                .underlayer_color = getColor(s.at("underlayer_color")),
                .underlayer_width = getDouble(s.at("underlayer_width")),
                .color_palette = getPallet(s.at("color_palette"))
            });
        }
    }

    template<class RequestType>
    auto ReadRequests(std::istream &in_stream = std::cin) {
        std::list<std::unique_ptr<RequestType>> requests;
        const size_t request_count = ReadNumberOnLine<size_t>(in_stream);

        for (size_t i = 0; i < request_count; ++i) {
            std::string request_str;
            getline(in_stream, request_str);
            if (auto request = ParseRequest<RequestType>(request_str)) {
                requests.push_back(move(request));
            }
        }
        return requests;
    }

    template<class RequestType>
    auto ReadJsonRequests(const Array &in_data) {
        LOG_DURATION("ReadJsonRequests");
        std::list<std::unique_ptr<RequestType>> requests;

        for (auto &item: in_data) {
            if (auto request = ParseJsonRequest<RequestType>(item)) {
                requests.push_back(move(request));
            }
        }

        return requests;
    }

    template<class RequestContainer>
    void ProcessModifyRequest(const RequestContainer &requests, DataBase &db) {
        {
            TotalDuration process(" Process Modify Request");
            for (const auto &request_holder: requests) {
                ADD_DURATION(process);
                request_holder->Process(db);
            }
        }
        {
            LOG_DURATION("BuildRoutes");
            db.BuildRoutes();
        }

    }

    template<class RequestContainer>
    auto ProcessReadRequests(
            const RequestContainer &requests, const DataBase &db) {
        LOG_DURATION("ProcessReadRequests");
        std::list<std::unique_ptr<AbstractData>> responses;
        for (const auto &request: requests) {
            responses.push_back(request->Process(db));
        }
        return responses;
    }

    template<class ResponseContainer>
    auto ResponsesToDocument(const ResponseContainer &responses) {
        LOG_DURATION("ResponsesToDocument");
        auto json_responses = Array();
        json_responses.reserve(responses.size());

        for (const auto &response: responses) {
            json_responses.push_back(response->toJson());
        }

        return Document(json_responses);
    }

    template<class ResponseContainer>
    void PrintResponses(const ResponseContainer &responses,
                        std::ostream &out_stream = std::cout) {
        for (const auto &response: responses) {
            out_stream << *response << std::endl;
        }
    }
}

int main() {
    LOG_DURATION("Total")
    DataBase db;
    std::ostream& out = std::cout;
    out.precision(6);

#ifdef DEBUG
    std::ifstream ifs("input.json");
    std::istream& in = ifs;
#else
    std::istream& in = std::cin;
#endif

#ifdef PLAN_TEXT
    const auto modify_requests = ReadRequests<ModifyRequest>(in);
    const auto read_requests = ReadRequests<ReadRequest>(in);

    ProcessModifyRequest(modify_requests, db);
    PrintResponses(ProcessReadRequests(read_requests, db), out);
#else
    auto in_data = Load(in);
    auto& requests = in_data.GetRoot().AsObject();
    ReadSettings(requests, db);

    const auto modify_requests = ReadJsonRequests<ModifyRequest>(
            requests.at("base_requests").AsArray());
    const auto read_requests = ReadJsonRequests<ReadRequest>(
            requests.at("stat_requests").AsArray());

    ProcessModifyRequest(modify_requests, db);
    auto doc = ResponsesToDocument(ProcessReadRequests(read_requests, db));

    Save(doc, out);
#endif

    return 0;
}
