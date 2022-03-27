#include<unordered_map>
#include<map>
#include<string>
#include<set>
#include <sstream>

#include "request.h"
#include "common.h"
#include "route.h"
#include "svg.h"

using namespace Json;

namespace
{
using namespace busdb;

std::string MapToStr(const Svg::Document& map) {
    std::ostringstream ss;
    ss << map;

    std::string temp = ss.str(), res;
    res.reserve(temp.size());
    for (auto c: temp) {
        if (c == '"' || c == '\\') {
            res.push_back('\\');
        }
        res.push_back(c);
    }

    return res;
}

struct BusData: AbstractData {
    std::string name;
    std::shared_ptr<Route> route;

    BusData(int request_id, std::string name, std::shared_ptr<Route> route) :
            AbstractData(request_id), name(move(name)), route(route) {
    }

    std::ostream& toStream(std::ostream& out) const override {
        out << "Bus " << name << ": ";
        if (!route)
            out << "not found";
        else
            out << *route;

        return out;
    }

    Object toJsonObject() const override {
        return route ? route->ToJsonObject() : Object {{"error_message", "not found" } };
    }
};

struct StopData: AbstractData {
    std::string name;
    std::optional<std::set<std::string_view>> buses;

    StopData(int request_id, std::string name, decltype(buses) buses) :
            AbstractData(request_id), name(move(name)), buses(buses) {
    }

    std::ostream& toStream(std::ostream& out) const override {
        out << "Stop " << name << ": ";
        if (!buses)
            out << "not found";
        else if (buses->empty())
            out << "no buses";
        else {
            out << "buses ";
            for (const auto& bus : *(buses)) {
                out << bus << ' ';
            }
        }
        return out;
    }

    Object toJsonObject() const override {
        if (!buses)
            return { {"error_message", "not found"}};

        auto b = Array();
        for (const auto& bus : *(buses)) {
            b.push_back(std::string(bus));
        }

        return {{"buses", move(b)}};
    }
};

struct RouteData: AbstractData {
    std::tuple<double, DataBase::StopsRoute, Svg::Document> data;

    RouteData(int request_id, decltype(data)&& data) :
            AbstractData(request_id), data(move(data)) {
    }

    std::ostream& toStream(std::ostream& out) const override {
        // do nothing, not applicable
        return out;
    }

    Object toJsonObject() const override {
        const auto& [total_time, route, map] = data;
        if (total_time < 0) return {{"error_message", "not found" }};

        Object res  = {{"total_time",  total_time}};
        Array items;
        for(const auto& [type, time, name, span_count]: route) {
            Object item = {{"type", TypeToString(type)}, {"time", time}};
            if(type == DataBase::RouteItemType::BUS) {
                item["bus"] = std::string(name);
                item["span_count"] = span_count;
            } else if (type == DataBase::RouteItemType::WAIT) {
                item["stop_name"] = std::string(name);
            }
            items.push_back(move(item));
        }

        res["items"] = move(items);
        res["map"] = MapToStr(map);

        return res;
    }

    std::string TypeToString(DataBase::RouteItemType type) const {
        return type == DataBase::RouteItemType::BUS ? "Bus" : "Wait";
    }
};


struct MapData: AbstractData {

    Svg::Document map;
    MapData(int request_id, Svg::Document&& map) :
            AbstractData(request_id), map(std::move(map)) {
    }

    std::ostream& toStream(std::ostream& out) const override {
        return out << map;
    }

    Object toJsonObject() const override {
        return {{"map", MapToStr(map)}};
    }
};

const std::unordered_map<std::string_view, Request::Type> STR_TO_REQUEST_TYPE = {
        { "Stop", Request::Type::STOP },
        { "Bus", Request::Type::BUS },
        { "Route", Request::Type::ROUTE},
        {"Map", Request::Type::MAP}
};


struct BusReadRequest: ReadRequest {
    BusReadRequest() :
            ReadRequest(Type::BUS) {
    }
    void ParseFrom(std::string_view input) override {
        name = std::string(input);
    }

    void ParseOther(const Object& data) override {
        name = data.at("name").AsString();
    }

    std::unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return std::make_unique<BusData>(id, name, db.GetBusRoute(name));
    }

    std::string name;
};

struct StopReadRequest: ReadRequest {
    StopReadRequest() :
            ReadRequest(Type::STOP) {
    }
    void ParseFrom(std::string_view input) override {
        name = std::string(input);
    }

    void ParseOther(const Object& data) override {
        name = data.at("name").AsString();
    }

    std::unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return std::make_unique<StopData>(id, name, db.GetStopBuses(name));
    }

    std::string name;
};

struct RouteReadRequest: ReadRequest {
    RouteReadRequest() :
            ReadRequest(Type::ROUTE) {
    }
    void ParseFrom(std::string_view input) override {
        // do nothing,
    }

    void ParseOther(const Object& data) override {
        from = data.at("from").AsString();
        to = data.at("to").AsString();
    }

    std::unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return std::make_unique<RouteData>(id, db.GetRoute(from, to));
    }

    std::string from, to;
};

struct MapReadRequest: ReadRequest {
    MapReadRequest() : ReadRequest(Type::MAP) {
    }

    void ParseFrom(std::string_view input) override {
    }

    void ParseOther(const Object& data) override {
    }

    std::unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return std::make_unique<MapData>(id, db.BuildMap());
    }
};

struct StopModifyRequest: ModifyRequest {
    StopModifyRequest() :
            ModifyRequest(Type::STOP) {
    }

    void ParseFrom(std::string_view input) override {
        name = std::string(ReadToken(input, ": "));
        position.latitude = ConvertToDouble(ReadToken(input, ", "));
        position.longitude = ConvertToDouble(ReadToken(input, ", "));

        while (!input.empty()) {
            auto temp = ReadToken(input, ", ");
            auto distance = ConvertToInt(ReadToken(temp, "m to "));
            distances.push_back({std::string(temp), distance});
        }
    }

    void ParseFrom(const Object& data) override {
        name = data.at("name").AsString();
        auto &latitude = data.at("latitude"), &longitude = data.at("longitude");
        position = {latitude.index() == (int)Node::Type::DoubleType ? latitude.AsDouble() : latitude.AsInt(),
                    longitude.index() == (int)Node::Type::DoubleType ? longitude.AsDouble() : longitude.AsInt()};

        for (auto& [stop_name, distance] : data.at("road_distances").AsObject()) {
            distances.push_back({stop_name, distance.AsInt()});
        }
    }

    void Process(DataBase& db) const override {
        db.AddStop(move(name), position, move(distances));
    }

    std::string name;
    Point position;
    std::list<std::pair<std::string, int>> distances;
};

struct BusModifyRequest: ModifyRequest {
    BusModifyRequest() :
            ModifyRequest(Type::BUS) {
    }
    void ParseFrom(std::string_view input) override {
        name = std::string(ReadToken(input, ": "));
        route = Route::ParseRoute(input);
    }

    void ParseFrom(const Object& data) override {
        name = data.at("name").AsString();
        route = Route::ParseRoute(data);
    }

    void Process(DataBase& db) const override {
        db.AddBus(move(name), move(route));
    }

    std::string name;
    std::shared_ptr<Route> route;
};

}

namespace busdb {

AbstractData::AbstractData(int request_id) :
        request_id(request_id) {
}

Node AbstractData::toJson() const {
    auto res = toJsonObject();
    res["request_id"] = request_id;
    return res;
}

std::ostream& operator<<(std::ostream& out, const AbstractData& data) {
    return data.toStream(out);
}

Request::Request(Type type): type(type) {
}

std::optional<Request::Type> ConvertRequestTypeFromString(std::string_view type_str) {
    if (const auto it = STR_TO_REQUEST_TYPE.find(type_str); it
            != STR_TO_REQUEST_TYPE.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ReadRequest::ParseFrom(const Object& data) {
    id = data.at("id").AsInt();
    ParseOther(data);
}

std::unique_ptr<ModifyRequest> ModifyRequest::Create(Request::Type type) {
    switch (type) {
        case Request::Type::STOP:
            return std::make_unique<StopModifyRequest>();
        case Request::Type::BUS:
            return std::make_unique<BusModifyRequest>();
    }

    return nullptr;
}

std::unique_ptr<ReadRequest> ReadRequest::Create(Request::Type type) {
    switch (type) {
        case Request::Type::BUS:
            return std::make_unique<BusReadRequest>();
        case Request::Type::STOP:
            return std::make_unique<StopReadRequest>();
        case Request::Type::ROUTE:
            return std::make_unique<RouteReadRequest>();
        case Request::Type::MAP:
            return std::make_unique<MapReadRequest>();
    }

    return nullptr;
}

}
