#include "common.h"
#include <cmath>
#include <sstream>


using namespace std;
namespace busdb {

pair<string_view, optional<string_view>> SplitTwoStrict(string_view s, string_view delimiter) {
  const size_t pos = s.find(delimiter);
  if (pos == s.npos) {
    return {s, nullopt};
  } else {
    return {s.substr(0, pos), s.substr(pos + delimiter.length())};
  }
}

pair<string_view, string_view> SplitTwo(string_view s, string_view delimiter) {
  const auto [lhs, rhs_opt] = SplitTwoStrict(s, delimiter);
  return {lhs, rhs_opt.value_or("")};
}

string_view ReadToken(string_view& s, string_view delimiter) {
  const auto [lhs, rhs] = SplitTwo(s, delimiter);
  s = rhs;
  return lhs;
}

int ConvertToInt(string_view str) {
  // use std::from_chars when available to git rid of string copy
  size_t pos;
  const int result = stoi(string(str), &pos);
  if (pos != str.length()) {
    std::stringstream error;
    error << "string " << str << " contains " << (str.length() - pos) << " trailing chars";
    throw invalid_argument(error.str());
  }
  return result;
}

double ConvertToDouble(string_view str) {
  // use std::from_chars when available to git rid of string copy
  size_t pos;
  const auto result = stod(string(str), &pos);
  if (pos != str.length()) {
    std::stringstream error;
    error << "string " << str << " contains " << (str.length() - pos) << " trailing chars";
    throw invalid_argument(error.str());
  }
  return result;
}

int Point::R = 6371000;

Point Point::ToRadians() const {
    return {latitude * ONE_DEGREE, longitude * ONE_DEGREE};
}

Point operator-(const Point& l, const Point& r) {
    return {l.latitude - r.latitude, l.longitude - r.longitude};
}

DistanceType Distance(const Point& l, const Point& r) {
    auto l_r = l.ToRadians(), r_r = r.ToRadians();
    auto diff_r = l_r - r_r;

    auto ans = pow(sin(diff_r.latitude / 2), 2) +
                          cos(l_r.latitude) * cos(r_r.latitude) *
                          pow(sin(diff_r.longitude / 2), 2);

    return 2 * asin(sqrt(ans)) * Point::R;
}

}
