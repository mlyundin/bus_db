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

    db.SetRouteSettings(requests);
    const auto modify_requests = ReadJsonRequests<ModifyRequest>(
            requests.at("base_requests").AsArray());
    const auto read_requests = ReadJsonRequests<ReadRequest>(
            requests.at("stat_requests").AsArray());

    ProcessModifyRequest(modify_requests, db);
    // Important should be after fill data in db
    db.SetRenderSettings(requests);

    auto doc = ResponsesToDocument(ProcessReadRequests(read_requests, db));
    Save(doc, out);
#endif

    return 0;
}
