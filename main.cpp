#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#include "common.h"
#include "database.h"
#include "request.h"
#include "json.h"
#include "code_profile.h"

using namespace std;
using namespace busdb;
using namespace Json;


void ReadSettings(const Object& in_data, DataBase& db) {
    if(in_data.count("routing_settings")) {
         const auto& settings = in_data.at("routing_settings").AsObject();
         db.SetSettings({settings.at("bus_wait_time").AsInt(),
                         settings.at("bus_velocity").AsInt()});
     }
}

template<class RequestType> auto ReadRequests(
        istream& in_stream = cin) {

    list<unique_ptr<RequestType>> requests;
    const size_t request_count = ReadNumberOnLine<size_t>(in_stream);

    for (size_t i = 0; i < request_count; ++i) {
        string request_str;
        getline(in_stream, request_str);
        if (auto request = ParseRequest<RequestType>(request_str)) {
            requests.push_back(move(request));
        }
    }
    return requests;
}

template<class RequestType> auto ReadJsonRequests(
        const Array& in_data) {
    LOG_DURATION("ReadJsonRequests");
    list<unique_ptr<RequestType>> requests;

    for (auto& item : in_data) {
        if (auto request = ParseJsonRequest<RequestType>(item)) {
            requests.push_back(move(request));
        }
    }

    return requests;
}

template <class RequestContainer>
void ProcessModifyRequest(const RequestContainer& requests,
        DataBase& db) {
    {
        TotalDuration process(" Process Modify Request");
        for (const auto& request_holder : requests) {
            ADD_DURATION(process);
            request_holder->Process(db);
        }
    }
    LOG_DURATION("BuildRoutes");
    db.BuildRoutes();
}

template<class RequestContainer> auto ProcessReadRequests(
        const RequestContainer& requests, const DataBase& db) {
    LOG_DURATION("ProcessReadRequests");
    list<unique_ptr<AbstractData>> responses;
    for (const auto& request : requests) {
        responses.push_back(request->Process(db));
    }
    return responses;
}

template<class ResponseContainer> auto ResponsesToDocument(const ResponseContainer& responses) {
    LOG_DURATION("ResponsesToDocument");
    auto json_responses = Array();
    json_responses.reserve(responses.size());

    for (const auto& response : responses) {
        json_responses.push_back(response->toJson());
    }

    return Document(json_responses);
}

template<class ResponseContainer>
void PrintResponses(const ResponseContainer& responses,
        ostream& out_stream = cout) {
    for (const auto& response : responses) {
        out_stream << *response << endl;
    }
}

int main() {

    LOG_DURATION("Total")
    DataBase db;
    ostream& out = cout;
#ifdef DEBUG
    ifstream ifs("test_input.txt");
    istream& in = ifs;
#else
    istream& in = cin;
#endif

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

        auto doc = ResponsesToDocument(ProcessReadRequests(read_requests, db));

#ifdef DEBUG
        ifstream output("test_output.txt");
        auto to_skip = unordered_set<string>{"items"};
        assert(EqualWithSkip(doc.GetRoot(), Load(output).GetRoot(), to_skip));
#endif
        Save(doc, out);
    } else {
        const auto modify_requests = ReadRequests<ModifyRequest>(in);
        const auto read_requests = ReadRequests<ReadRequest>(in);

        ProcessModifyRequest(modify_requests, db);
        PrintResponses(ProcessReadRequests(read_requests, db), out);
    }

    return 0;
}
