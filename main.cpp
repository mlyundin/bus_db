#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#include "common.h"
#include "database.h"
#include "request.h"

using namespace std;
using namespace busdb;

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

void ProcessModifyRequest(const vector<unique_ptr<ModifyRequest>>& requests, DataBase& db) {
	for (const auto& request_holder : requests) {
		request_holder->Process(db);
	}
}

template<class Request> vector<unique_ptr<AbstractData>> ProcessReadRequests(
		const vector<unique_ptr<Request>>& requests, const DataBase& db) {
	vector<unique_ptr<AbstractData>> responses;
	for (const auto& request_holder : requests) {
		responses.push_back(request_holder->Process(db));
	}
	return responses;
}

void PrintResponses(const vector<unique_ptr<AbstractData>>& responses, ostream& out_stream = cout) {
	for (const auto& response : responses) {
		out_stream << *response << endl;
	}
}

int main() {

	DataBase db;
 	istream& in = cin;

//	ifstream ifs("test_input.txt");
//	istream& in = ifs;

	cout.precision(6);
	const auto modify_requests = ReadRequests<ModifyRequest>(in);
	const auto read_requests = ReadRequests<ReadRequest>(in);

	ProcessModifyRequest(modify_requests, db);
	PrintResponses(ProcessReadRequests(read_requests, db));

	return 0;
}
