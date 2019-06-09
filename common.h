#pragma once

#include<string_view>
#include<optional>
#include<iostream>
#include<string>

namespace busdb {

std::pair<std::string_view, std::optional<std::string_view>> SplitTwoStrict(
        std::string_view s, std::string_view delimiter = " ");

std::pair<std::string_view, std::string_view> SplitTwo(std::string_view s,
        std::string_view delimiter = " ");

std::string_view ReadToken(std::string_view& s,
        std::string_view delimiter = " ");

int ConvertToInt(std::string_view str);

double ConvertToDouble(std::string_view str);

template<typename Number>
Number ReadNumberOnLine(std::istream& stream) {
    Number number;
    stream >> number;
    std::string dummy;
    getline(stream, dummy);
    return number;
}

using CoordinateType = double;
using DistanceType = CoordinateType;

struct Point {
    static constexpr DistanceType ONE_DEGREE = 3.1415926535 / 180;
    static int R;

    CoordinateType latitude, longitude;

    void ToRadians();
};

Point operator-(const Point& l, const Point& r);

DistanceType Distance(Point l, Point r);

struct hash_pair {
    template <class T1, class T2>
    size_t operator()(const std::pair<T1, T2>& p) const
    {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ hash2;
    }
};

}
;
