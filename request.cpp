#include<unordered_map>
#include<map>
#include<string>
#include<set>

#include "request.h"
#include "common.h"
#include "route.h"

using namespace std;
using namespace Json;

namespace busdb {

AbstractData::AbstractData(int request_id) :
        request_id(request_id) {
}

Node AbstractData::toJson() const {
    auto res = toJsonObject();
    res["request_id"] = request_id;
    return res;
}

ostream& operator<<(ostream& out, const AbstractData& data) {
    return data.toStream(out);
}

struct BusData: AbstractData {
    string name;
    shared_ptr<Route> route;

    BusData(int request_id, string name, shared_ptr<Route> route) :
            AbstractData(request_id), name(move(name)), route(route) {
    }

    ostream& toStream(ostream& out) const override {
        out << "Bus " << name << ": ";
        if (!route)
            out << "not found";
        else
            out << *route;

        return out;
    }

    Object toJsonObject() const override {
        return route ? route->toJsonObject() : Object { { "error_message",
                "not found"s } };
    }
};

struct StopData: AbstractData {
    string name;
    optional<set<string_view>> buses;

    StopData(int request_id, string name, optional<set<string_view>> buses) :
            AbstractData(request_id), name(move(name)), buses(buses) {
    }

    ostream& toStream(ostream& out) const override {
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
            return { {"error_message", "not found"s}};

        auto b = Array();
        for (const auto& bus : *(buses)) {
            b.push_back(string(bus));
        }

        return { {"buses", move(b)}};
    }
};

struct RouteData: AbstractData {
    tuple<double, list<DataBase::RouteItem>> data;

    RouteData(int request_id, tuple<double, list<DataBase::RouteItem>> data) :
            AbstractData(request_id), data(move(data)) {
    }

    ostream& toStream(ostream& out) const override {
        // do nothing, not applicable
        return out;
    }

    Object toJsonObject() const override {
        const auto& [total_time, route] = data;
        if (total_time < 0) return {{"error_message", "not found"s }};

        Object res  = {{"total_time",  total_time}};
        Array items;
        for(const auto& [type, time, name, span_count]: route) {
            Object item = {{"type", TypeToString(type)}, {"time", time}};
            if(type == DataBase::RouteItemType::BUS) {
                item["bus"] = string(name);
                item["span_count"] = span_count;
            } else if (type == DataBase::RouteItemType::WAIT) {
                item["stop_name"] = string(name);
            }
            items.push_back(move(item));
        }

        res["items"] = move(items);
        return res;
    }

    string TypeToString(DataBase::RouteItemType type) const {
        return type == DataBase::RouteItemType::BUS ? "Bus" : "Wait";
    }
};

Request::Request(Type type) :
        type(type) {
}

const unordered_map<string_view, Request::Type> STR_TO_REQUEST_TYPE = {
        { "Stop", Request::Type::STOP },
        { "Bus", Request::Type::BUS },
        { "Route", Request::Type::ROUTE}};

optional<Request::Type> ConvertRequestTypeFromString(string_view type_str) {
    if (const auto it = STR_TO_REQUEST_TYPE.find(type_str); it
            != STR_TO_REQUEST_TYPE.end()) {
        return it->second;
    }
    return nullopt;
}

void ReadRequest::ParseFrom(const Object& data) {
    id = data.at("id").AsInt();
    ParseOther(data);
}

struct BusReadRequest: ReadRequest {
    BusReadRequest() :
            ReadRequest(Type::BUS) {
    }
    void ParseFrom(string_view input) override {
        name = string(input);
    }

    void ParseOther(const Object& data) override {
        name = data.at("name").AsString();
    }

    unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return make_unique<BusData>(id, name, db.GetBusRoute(name));
    }

    string name;
};

struct StopReadRequest: ReadRequest {
    StopReadRequest() :
            ReadRequest(Type::STOP) {
    }
    void ParseFrom(string_view input) override {
        name = string(input);
    }

    void ParseOther(const Object& data) override {
        name = data.at("name").AsString();
    }

    unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return make_unique<StopData>(id, name, db.GetStopBuses(name));
    }

    string name;
};

struct RouteReadRequest: ReadRequest {
    RouteReadRequest() :
            ReadRequest(Type::ROUTE) {
    }
    void ParseFrom(string_view input) override {
        // do nothing,
    }

    void ParseOther(const Object& data) override {
        from = data.at("from").AsString();
        to = data.at("to").AsString();
    }

    unique_ptr<AbstractData> Process(const DataBase& db) const override {
        return make_unique<RouteData>(id, db.GetRoute(from, to));
    }

    string from, to;
};

struct StopModifyRequest: ModifyRequest {
    StopModifyRequest() :
            ModifyRequest(Type::STOP) {
    }

    void ParseFrom(string_view input) override {
        name = string(ReadToken(input, ": "));
        position.latitude = ConvertToDouble(ReadToken(input, ", "));
        position.longitude = ConvertToDouble(ReadToken(input, ", "));

        while (!input.empty()) {
            auto temp = ReadToken(input, ", ");
            auto distance = ConvertToInt(ReadToken(temp, "m to "));
            distances.push_back({string(temp), distance});
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

    string name;
    Point position;
    list<pair<string, int>> distances;
};

struct BusModifyRequest: ModifyRequest {
    BusModifyRequest() :
            ModifyRequest(Type::BUS) {
    }
    void ParseFrom(string_view input) override {
        name = string(ReadToken(input, ": "));
        route = Route::ParseRoute(input);
    }

    void ParseFrom(const Object& data) override {
        name = data.at("name").AsString();
        route = Route::ParseRoute(data);
    }

    void Process(DataBase& db) const override {
        db.AddBus(move(name), move(route));
    }

    string name;
    shared_ptr<Route> route;
};

unique_ptr<ModifyRequest> ModifyRequest::Create(Request::Type type) {
    switch (type) {
    case Request::Type::STOP:
        return make_unique<StopModifyRequest>();
    case Request::Type::BUS:
        return make_unique<BusModifyRequest>();
    default:
        return nullptr;
    }
}

unique_ptr<ReadRequest> ReadRequest::Create(Request::Type type) {
    switch (type) {
    case Request::Type::BUS:
        return make_unique<BusReadRequest>();
    case Request::Type::STOP:
        return make_unique<StopReadRequest>();
    case Request::Type::ROUTE:
        return make_unique<RouteReadRequest>();
    default:
        return nullptr;
    }
}

}
