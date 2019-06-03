#include "json.h"

#include<sstream>
#include<stdexcept>
#include<climits>
using namespace std;

namespace Json {

Document::Document(Node root) :
        root(move(root)) {
}

const Node& Document::GetRoot() const {
    return root;
}

Node LoadNode(istream& input);

Node LoadArray(istream& input) {
    vector<Node> result;

    for (char c; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }

    return Node(move(result));
}

Node LoadInt(istream& input) {
    int result = 0;
    while (isdigit(input.peek())) {
        result *= 10;
        result += input.get() - '0';
    }
    return Node(result);
}

Node LoadString(istream& input) {
    string line;
    getline(input, line, '"');
    return Node(move(line));
}

Node LoadObject(istream& input) {
    map<string, Node> result;

    for (char c; input >> c && c != '}';) {
        if (c == ',') {
            input >> c;
        }

        string key = LoadString(input).AsString();
        input >> c;
        result.emplace(move(key), LoadNode(input));
    }

    return Node(move(result));
}

Node LoadLiteral(istream& input) {
    stringstream ss;

    for (char c; input >> c;) {
        if (c == ',' || c == '}' || c == ']') {
            input.putback(c);
            break;
        }
        ss << c;
    }

    auto s = ss.str();
    Node res;

    if (s.empty()) {
        throw invalid_argument("empty");
    } else if (s == "true") {
        res = Node(true);
    } else if (s == "false") {
        res = Node(false);
    } else {
        auto value = stod(s);
        res = (value >= 0 && value < INT_MAX && s.find('.') == string::npos) ?
                Node(int(value)) : Node(value);
    }

    return res;
}

Node LoadNode(istream& input) {

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

Document Load(istream& input) {
    return Document { LoadNode(input) };
}

ostream& operator<<(ostream& output, const Node& node) {
    switch (node.index()) {
    case 0:
        output << '[';

        if (auto& res = node.AsArray(); !res.empty()) {
            auto it = res.begin(), end = --res.end();
            for (; it != end; ++it)
                output << *it << ", ";
            output << *it;
        }

        output << ']';
        break;

    case 1:
        output << '{';

        if (auto& res = node.AsObject(); !res.empty()) {
            auto it = res.begin(), end = --res.end();
            for (; it != end; ++it)
                output << '"' << it->first << "\": " << it->second << ",\n";
            output << '"' << it->first << "\": " << it->second << "\n";
        }

        output << "}\n";
        break;

    case 2:
        output << node.AsInt();
        break;
    case 3:
        output << node.AsDouble();
        break;
    case 4:
        output << (node.AsBoolean() ? "true" : "false");
        break;
    case 5:
        output << '"' << node.AsString() << '"';
        break;
    }

    return output;
}

void Save(const Document& document, ostream& output) {
    output << document.GetRoot();
}

}
