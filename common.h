#pragma once

#include<string_view>
#include<optional>
#include<iostream>
#include<string>
#include<vector>

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

template <typename Iterator>
class IteratorRange {
public:
  IteratorRange(Iterator begin, Iterator end)
    : first(begin)
    , last(end)
    , size_(distance(first, last))
  {
  }

  Iterator begin() const {
    return first;
  }

  Iterator end() const {
    return last;
  }

  size_t size() const {
    return size_;
  }

private:
  Iterator first, last;
  size_t size_;
};

template <typename Iterator>
class Paginator {
private:
  std::vector<IteratorRange<Iterator>> pages;

public:
  Paginator(Iterator begin, Iterator end, size_t page_size) {
    for (size_t left = distance(begin, end); left > 0; ) {
      size_t current_page_size = std::min(page_size, left);
      Iterator current_page_end = next(begin, current_page_size);
      pages.push_back({begin, current_page_end});

      left -= current_page_size;
      begin = current_page_end;
    }
  }

  auto begin() const {
    return pages.begin();
  }

  auto end() const {
    return pages.end();
  }

  size_t size() const {
    return pages.size();
  }
};

template <typename C>
auto Paginate(C& c, size_t page_size) {
  return Paginator(begin(c), end(c), page_size);
}


}
