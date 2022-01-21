#include "code_profile.h"
#include <iostream>

TotalDuration::TotalDuration(const std::string& msg): message(msg + ": "), value(0) {}

TotalDuration::~TotalDuration() {
    std::cerr << message << std::chrono::duration_cast<std::chrono::milliseconds>(value).count() << " ms"<< std::endl;
}


AddDuration::AddDuration(std::chrono::steady_clock::duration& dest): add_to(dest),
start(std::chrono::steady_clock::now()) {}

AddDuration::AddDuration(TotalDuration& dest): AddDuration(dest.value) {}

AddDuration::~AddDuration() {
    add_to += std::chrono::steady_clock::now() - start;
}

LogDuration::LogDuration(const std::string& msg): message(msg + ": "),
    start(std::chrono::steady_clock::now()) {}

LogDuration::~LogDuration() {
  auto finish = std::chrono::steady_clock::now();
  auto dur = finish - start;
  std::cerr << message
     << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
     << " ms" << std::endl;
}
