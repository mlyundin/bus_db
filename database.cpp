#include <optional>
#include <memory>
#include <string>

#include "database.h"
#include "route.h"

using namespace std;

namespace busdb {

DistanceType DataBase::LineDistance(const string& stop1, const string& stop2) const {
	return busdb::Distance(stops_.at(stop1), stops_.at(stop2));
}

DistanceType DataBase::Distance(const string& stop1, const string& stop2) const {
    if (auto it1 = distance_hash_.find(stop1);it1 != distance_hash_.end()) {
    	if (auto it2 = it1->second.find(stop2); it2 != it1->second.end()) {
    		return it2->second;
    	}
    }

    DistanceType res = {};
    auto it_s1 = stops_.find(stop1), it_s2 = stops_.find(stop2);
    if (it_s1 != stops_.end() && it_s2 != stops_.end()) {
        res = distance_hash_[it_s1->first][it_s2->first] = LineDistance(stop1, stop2);
    }

    return res;
}

void DataBase::AddStop(string stop, Point location,
		const unordered_map<string, int>& distances) {
	stops_[stop] = location;
	auto it = stops_.find(stop);

	string_view stop_name = it->first;
	if(stop_buses_.count(stop_name) == 0) {
		stop_buses_[stop_name];
	}

	for(const auto& [another_stop_name, distance]: distances) {
	    auto [another_it, temp] = stops_.insert({another_stop_name, {}});
		distance_hash_[it->first][another_it->first] = distance;

		auto& hash = distance_hash_[another_it->first];
		hash.insert({it->first, distance});
	}
}

void DataBase::AddBus(string number, shared_ptr<Route> route) {
    route->SetDB(this);
    auto [it, inserted] = buses_.insert({move(number), move(route)});

    if (inserted) {
		string_view bus_number = it->first;
		for(const auto& stop: *(it->second)) {
			stop_buses_[stop].insert(bus_number);
		}
    }
}

shared_ptr<Route> DataBase::GetBusRoute(const string& number) const {
    shared_ptr<Route> res = nullptr;
    if(auto it = buses_.find(number); it != buses_.end()) res = it->second;

    return res;
}

optional<set<string_view>> DataBase::GetStopBuses(string_view stop) const {
	auto it = stop_buses_.find(stop);
	if (it == stop_buses_.end()) return nullopt;

	return it->second;
}

}
