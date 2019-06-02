#pragma once

#include <istream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace Json {
	class Node;
	using Object = std::map<std::string, Node>;
	using Array = std::vector<Node>;

  class Node : std::variant<Array,
                            Object,
                            int,
                            double,
                            bool,
                            std::string> {
  public:
	enum Type {
		ArrayType = 0,
		ObjectType,
		IntType,
		DoubleType,
		BooleanType,
		StringType
	};

    using variant::variant;
    using variant::index;

    const auto& AsArray() const {
      return std::get<Array>(*this);
    }
    const auto& AsObject() const {
      return std::get<Object>(*this);
    }
    int AsInt() const {
      return std::get<int>(*this);
    }
    bool AsBoolean() const {
      return std::get<bool>(*this);
    }
    double AsDouble() const {
      return std::get<double>(*this);
    }
    const auto& AsString() const {
      return std::get<std::string>(*this);
    }
  };

  class Document {
  public:
    explicit Document(Node root);

    const Node& GetRoot() const;

  private:
    Node root;
  };

  Document Load(std::istream& input);

  void Save(const Document& document, std::ostream& output);
}
