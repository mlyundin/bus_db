#include<unordered_map>
#include<string>
#include<set>

#include "request.h"
#include "common.h"
#include "route.h"

using namespace std;

namespace busdb {

ostream& operator<<(ostream& out, const AbstractData& data) {
	return data.toStream(out);
}

struct BusData: AbstractData {
    string number;
    shared_ptr<Route> route;

    BusData(string number, shared_ptr<Route> route): number(number), route(route){}

    ostream& toStream(ostream& out) const override {
        out<<"Bus "<<number<<": ";
        if (!route) out<<"not found";
        else out<<*route;

        return out;
    }
};

struct StopData: AbstractData {
	string_view name;
	optional<set<string_view>> buses;

	StopData(string_view name, optional<set<string_view>> buses) :
			name(name), buses(buses) {
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
};


Request::Request(Type type): type(type) {}

const unordered_map<string_view, Request::Type> STR_TO_REQUEST_TYPE = {
    {"Stop", Request::Type::STOP},
    {"Bus", Request::Type::BUS}
};

optional<Request::Type> ConvertRequestTypeFromString(string_view type_str) {
	if (const auto it = STR_TO_REQUEST_TYPE.find(type_str);
			it != STR_TO_REQUEST_TYPE.end()) {
		return it->second;
	}
	return nullopt;
}

struct BusReadRequest: ReadRequest {
	BusReadRequest() :
			ReadRequest(Type::BUS) {
	}
	void ParseFrom(string_view input) override {
		number = string(input);
	}

	unique_ptr<AbstractData> Process(const DataBase& db) const override {
		return make_unique<BusData>(number, db.GetBusRoute(number));
	}

	string number;
};

struct StopReadRequest: ReadRequest {
	StopReadRequest(): ReadRequest(Type::STOP) {
	}
	void ParseFrom(string_view input) override {
		name = string(input);
	}

	unique_ptr<AbstractData> Process(const DataBase& db) const override {
		return make_unique<StopData>(name, db.GetStopBuses(name));
	}

	string name;
};

struct StopModifyRequest: ModifyRequest {
	StopModifyRequest(): ModifyRequest(Type::STOP) {
	}

	void ParseFrom(string_view input) override {
		name = string(ReadToken(input, ": "));
		position.latitude = ConvertToDouble(ReadToken(input, ", "));
		position.longitude = ConvertToDouble(ReadToken(input, ", "));

		while (!input.empty()) {
			auto temp = ReadToken(input, ", ");
			auto distance = ConvertToInt(ReadToken(temp, "m to "));
			distances[string(temp)] = distance;
		}
	}

	void Process(DataBase& db) const override {
		db.AddStop(name, position, distances);
	}

	string name;
	Point position;
	unordered_map<string, int> distances;
};

struct BusModifyRequest: ModifyRequest {
	BusModifyRequest(): ModifyRequest(Type::BUS) {
	}
	void ParseFrom(string_view input) override {
		number = string(ReadToken(input, ": "));
		route = Route::ParseRoute(input);
	}

	void Process(DataBase& db) const override {
		db.AddBus(number, route);
	}

	string number;
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
	default:
		return nullptr;
	}
}

}
