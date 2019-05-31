#pragma once

#include <iostream>
#include <memory>
#include <string_view>
#include <optional>

#include "database.h"

namespace busdb {

struct AbstractData {
	virtual std::ostream& toStream(std::ostream& out) const = 0;
	virtual ~AbstractData() = default;
};

std::ostream& operator<<(std::ostream& out, const AbstractData& data);

struct Request {
  enum class Type {
    BUS,
    STOP
  };

  Request(Type type);
  virtual void ParseFrom(std::string_view input) = 0;
  virtual ~Request() = default;

  const Type type;
};

struct ReadRequest : Request {
  using Request::Request;
  static std::unique_ptr<ReadRequest> Create(Type type);
  virtual std::unique_ptr<AbstractData> Process(const DataBase& db) const = 0;
};

struct ModifyRequest : Request {
  using Request::Request;
  static std::unique_ptr<ModifyRequest> Create(Type type);
  virtual void Process(DataBase& db) const = 0;
};

std::optional<Request::Type> ConvertRequestTypeFromString(std::string_view type_str);

template<class RequestType> std::unique_ptr<RequestType> ParseRequest(
		std::string_view request_str) {
	const auto request_type = ConvertRequestTypeFromString(ReadToken(request_str));
	if (!request_type) {
		return nullptr;
	}

	auto request = RequestType::Create(*request_type);
	if (request) {
		request->ParseFrom(request_str);
	};
	return request;
}

}
