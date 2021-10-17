#pragma once
#include<string>
#include<chrono>

struct TotalDuration {
    std::string message;
    std::chrono::steady_clock::duration value;
    explicit TotalDuration(const std::string& msg = "");
    ~TotalDuration();
};

class AddDuration {
public:
    explicit AddDuration(std::chrono::steady_clock::duration& dest);
    explicit AddDuration(TotalDuration& dest);
    ~AddDuration();
private:
    std::chrono::steady_clock::duration& add_to;
    std::chrono::steady_clock::time_point start;
};

class LogDuration {
public:
  explicit LogDuration(const std::string& msg = "");

  ~LogDuration();
private:
  std::string message;
  std::chrono::steady_clock::time_point start;
};

#define UNIQ_ID_IMPL(lineno) _a_local_var_##lineno
#define UNIQ_ID(lineno) UNIQ_ID_IMPL(lineno)

#define ADD_DURATION(value) \
AddDuration UNIQ_ID(__LINE__){value};

#define LOG_DURATION(message) \
  LogDuration UNIQ_ID(__LINE__){message};
