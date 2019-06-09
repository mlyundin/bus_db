#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#include "common.h"
#include "database.h"
#include "request.h"
#include "json.h"

using namespace std;
using namespace busdb;
using namespace Json;

template<class RequestType> vector<unique_ptr<RequestType>> ReadRequests(
        istream& in_stream = cin) {
    const size_t request_count = ReadNumberOnLine<size_t>(in_stream);

    vector<unique_ptr<RequestType>> requests;
    requests.reserve(request_count);

    for (size_t i = 0; i < request_count; ++i) {
        string request_str;
        getline(in_stream, request_str);
        if (auto request = ParseRequest<RequestType>(request_str)) {
            requests.push_back(move(request));
        }
    }
    return requests;
}

template<class RequestType> vector<unique_ptr<RequestType>> ReadJsonRequests(
        const Array& in_data) {
    vector<unique_ptr<RequestType>> requests;
    requests.reserve(in_data.size());

    for (auto& item : in_data) {
        if (auto request = ParseJsonRequest<RequestType>(item)) {
            requests.push_back(move(request));
        }
    }

    return requests;
}

void ProcessModifyRequest(const vector<unique_ptr<ModifyRequest>>& requests,
        DataBase& db) {
    for (const auto& request_holder : requests) {
        request_holder->Process(db);
    }
    db.BuildRoutes();
}

template<class Request> vector<unique_ptr<AbstractData>> ProcessReadRequests(
        const vector<unique_ptr<Request>>& requests, const DataBase& db) {
    vector<unique_ptr<AbstractData>> responses;
    for (const auto& request_holder : requests) {
        responses.push_back(request_holder->Process(db));
    }
    return responses;
}

void PrintResponses(const vector<unique_ptr<AbstractData>>& responses,
        ostream& out_stream = cout) {
    for (const auto& response : responses) {
        out_stream << *response << endl;
    }
}

void PrintJsonResponses(const vector<unique_ptr<AbstractData>>& responses,
        ostream& out_stream = cout) {
    auto json_responses = Array();
    json_responses.reserve(responses.size());

    for (const auto& response : responses) {
        json_responses.push_back(response->toJson());
    }

    Save(Document(json_responses), out_stream);
}

void ReadSettings(const Object& in_data, DataBase& db) {
    if(in_data.count("routing_settings")) {
         auto& settings = in_data.at("routing_settings").AsObject();
         db.SetSettings({settings.at("bus_wait_time").AsInt(),
                         settings.at("bus_velocity").AsInt()});
     }
}

int main() {

    DataBase db;
    istream& in = cin;

//    ifstream ifs("test_input.txt");
//    istream& in = ifs;

    cout.precision(6);
    auto is_json = true;
    if (is_json) {
        auto in_data = Load(in);
        auto& requests = in_data.GetRoot().AsObject();
        ReadSettings(requests, db);

        const auto modify_requests = ReadJsonRequests<ModifyRequest>(
                requests.at("base_requests").AsArray());
        const auto read_requests = ReadJsonRequests<ReadRequest>(
                requests.at("stat_requests").AsArray());

        ProcessModifyRequest(modify_requests, db);
        PrintJsonResponses(ProcessReadRequests(read_requests, db));
    } else {
        const auto modify_requests = ReadRequests<ModifyRequest>(in);
        const auto read_requests = ReadRequests<ReadRequest>(in);

        ProcessModifyRequest(modify_requests, db);
        PrintResponses(ProcessReadRequests(read_requests, db));
    }

    return 0;
}
