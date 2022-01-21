#include "common.h"
#include <cmath>
#include <sstream>


namespace busdb {

std::pair<std::string_view, std::optional<std::string_view>>
SplitTwoStrict(std::string_view s, std::string_view delimiter) {
    const size_t pos = s.find(delimiter);
    if (pos == s.npos) {
        return {s, std::nullopt};
    } else {
        return {s.substr(0, pos), s.substr(pos + delimiter.length())};
    }
}

std::pair<std::string_view, std::string_view>
SplitTwo(std::string_view s, std::string_view delimiter) {
    const auto [lhs, rhs_opt] = SplitTwoStrict(s, delimiter);
    return {lhs, rhs_opt.value_or("")};
}

std::string_view ReadToken(std::string_view& s, std::string_view delimiter) {
    const auto [lhs, rhs] = SplitTwo(s, delimiter);
    s = rhs;
    return lhs;
}

int ConvertToInt(std::string_view str) {
    // use std::from_chars when available to git rid of string copy
    size_t pos;
    const int result = stoi(std::string(str), &pos);
    if (pos != str.length()) {
        std::stringstream error;
        error << "string " << str << " contains " << (str.length() - pos)
                << " trailing chars";
        throw std::invalid_argument(error.str());
    }
    return result;
}

double ConvertToDouble(std::string_view str) {
    // use std::from_chars when available to git rid of string copy
    size_t pos;
    const auto result = stod(std::string(str), &pos);
    if (pos != str.length()) {
        std::stringstream error;
        error << "string " << str << " contains " << (str.length() - pos)
                << " trailing chars";
        throw std::invalid_argument(error.str());
    }
    return result;
}

int Point::R = 6371000;

void Point::ToRadians() {
    latitude *= Point::ONE_DEGREE;
    longitude *= Point::ONE_DEGREE;
}

Point operator-(const Point& l, const Point& r) {
    return {l.latitude - r.latitude, l.longitude - r.longitude};
}

DistanceType Distance(Point l, Point r) {
    l.ToRadians();
    r.ToRadians();

    return std::acos(std::sin(l.latitude) * std::sin(r.latitude) + std::cos(l.latitude) * std::cos(r.latitude) *
                std::cos(std::abs(l.longitude - r.longitude))) * Point::R;
}

}
