#include "../include/code_profile.h"
#include <iostream>
#include <sstream>

using namespace std;
using namespace chrono;

TotalDuration::TotalDuration(const string& msg): message(msg + ": "), value(0) {}

TotalDuration::~TotalDuration() {
    cerr << message << duration_cast<milliseconds>(value).count() << " ms"<< endl;
}


AddDuration::AddDuration(steady_clock::duration& dest): add_to(dest), start(steady_clock::now()) {}

AddDuration::AddDuration(TotalDuration& dest): AddDuration(dest.value) {}

AddDuration::~AddDuration() {
    add_to += steady_clock::now() - start;
}

LogDuration::LogDuration(const string& msg): message(msg + ": "), start(steady_clock::now()) {}

LogDuration::~LogDuration() {
  auto finish = steady_clock::now();
  auto dur = finish - start;
  cerr << message
     << duration_cast<milliseconds>(dur).count()
     << " ms" << endl;
}
