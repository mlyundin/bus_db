#include "../include/json.h"

#include<sstream>
#include<stdexcept>
#include<climits>

namespace {

using namespace Json;

Node LoadNode(std::istream& input);

Node LoadArray(std::istream& input) {
    std::vector<Node> result;

    for (char c; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }

    return Node(move(result));
}

Node LoadInt(std::istream& input) {
    int result = 0;
    while (isdigit(input.peek())) {
        result *= 10;
        result += input.get() - '0';
    }
    return Node(result);
}

Node LoadString(std::istream& input) {
    std::string line;
    getline(input, line, '"');
    return Node(move(line));
}

Node LoadObject(std::istream& input) {
    std::map<std::string, Node> result;

    for (char c; input >> c && c != '}';) {
        if (c == ',') {
            input >> c;
        }

        auto key = LoadString(input).AsString();
        input >> c;
        result.emplace(move(key), LoadNode(input));
    }

    return Node(move(result));
}

Node LoadLiteral(std::istream& input) {
    std::stringstream ss;

    for (char c; input >> c;) {
        if (c == ',' || c == '}' || c == ']') {
            input.putback(c);
            break;
        }
        ss << c;
    }

    Node res;
    if (auto s = ss.str();s.empty()) {
        throw std::invalid_argument("empty");
    } else if (s == "true") {
        res = Node(true);
    } else if (s == "false") {
        res = Node(false);
    } else {
        auto value = stod(s);
        res = (value >= 0 && value < INT_MAX && s.find('.') == std::string::npos) ?
              Node(int(value)) : Node(value);
    }

    return res;
}

Node LoadNode(std::istream& input) {

    char c;
    input >> c;

    if (c == '[') {
        return LoadArray(input);
    } else if (c == '{') {
        return LoadObject(input);
    } else if (c == '"') {
        return LoadString(input);
    } else {
        input.putback(c);
        return LoadLiteral(input);
    }
}

bool EqualWithSkip(const Node& left, const Node& right,
                   std::optional<std::unordered_set<std::string>> attr_to_skip = std::nullopt);

bool EqualWithSkip(const Array& left, const Array& right,
                   std::optional<std::unordered_set<std::string>> attr_to_skip) {
    if(left.size() != right.size()) return false;

    for(auto i = 0; i < left.size(); ++i) {
        if (!EqualWithSkip(left[i], right[i], attr_to_skip)) return false;
    }

    return true;
}

bool EqualWithSkip(const Object& left, const Object& right,
                   std::optional<std::unordered_set<std::string>> attr_to_skip) {
    if(left.size() != right.size()) return false;

    for(const auto& [left_name, left_node]: left) {
        if(auto right_item_it = right.find(left_name); !(attr_to_skip &&
                                                         attr_to_skip->count(left_name)) &&
                                                       (right_item_it == right.end() ||
                                                        !(left_node == right_item_it->second))) {
            return false;
        }
    }

    return true;
}

bool EqualWithSkip(const Node& left, const Node& right,
                   std::optional<std::unordered_set<std::string>> attr_to_skip) {

    if (left.index() != right.index())
        return false;

    switch ((Node::Type)left.index()) {
        case Node::Type::ArrayType:
            return EqualWithSkip(left.AsArray(), right.AsArray(), attr_to_skip);
        case Node::Type::ObjectType:
            return EqualWithSkip(left.AsObject(), right.AsObject(), attr_to_skip);
        case Node::Type::IntType:
            return left.AsInt() == right.AsInt();
        case Node::Type::DoubleType:
            return abs(left.AsDouble() - right.AsDouble()) < 0.0001;
        case Node::Type::BooleanType:
            return left.AsBoolean() == right.AsBoolean();
        case Node::Type::StringType:
            return left.AsString() == right.AsString();
    }
}

}

namespace Json {

Document::Document(Node root) :
        root(move(root)) {
}

const Node& Document::GetRoot() const {
    return root;
}

Document Load(std::istream& input) {
    return Document { LoadNode(input) };
}

std::ostream& operator<<(std::ostream& output, const Node& node) {
    switch ((Node::Type)node.index()) {
    case Node::Type::ArrayType:
        output << '[';

        if (auto& res = node.AsArray(); !res.empty()) {
            auto it = res.begin(), end = --res.end();
            for (; it != end; ++it)
                output << *it << ", ";
            output << *it;
        }

        output << ']';
        break;

    case Node::Type::ObjectType:
        output << '{';

        if (auto& res = node.AsObject(); !res.empty()) {
            auto it = res.begin(), end = --res.end();
            for (; it != end; ++it)
                output << '"' << it->first << "\": " << it->second << ",\n";
            output << '"' << it->first << "\": " << it->second << "\n";
        }

        output << "}\n";
        break;

    case Node::Type::IntType:
        output << node.AsInt();
        break;
    case Node::Type::DoubleType:
        output << node.AsDouble();
        break;
    case Node::Type::BooleanType:
        output << (node.AsBoolean() ? "true" : "false");
        break;
    case Node::Type::StringType:
        output << '"' << node.AsString() << '"';
        break;
    }

    return output;
}

void Save(const Document& document, std::ostream& output) {
    output << document.GetRoot();
}

bool EqualWithSkip(const Document& left, const Document& right,
                   std::optional<std::unordered_set<std::string>> attr_to_skip) {
    return ::EqualWithSkip(left.GetRoot(), right.GetRoot(), attr_to_skip);
}

bool operator == (const Node& left, const Node& right) {
    return ::EqualWithSkip(left, right);
}

}
