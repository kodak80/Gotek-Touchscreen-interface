/* Minimal Arduino shim for compiling gti_ipf.h on Linux (tests only). */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <string>
#include <chrono>

class String {
public:
    std::string s;
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(const std::string &x) : s(x) {}
    unsigned length() const { return (unsigned)s.size(); }
    void remove(unsigned i) { s.erase(i); }
    const char *c_str() const { return s.c_str(); }
    String operator+(const String &o) const { return String(s + o.s); }
    String operator+(const char *o) const { return String(s + o); }
    friend String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }
};

struct SerialShim {
    template <typename... A> void printf(const char *f, A... a) { ::printf(f, a...); }
};
static SerialShim Serial;

static inline unsigned long millis()
{
    using namespace std::chrono;
    return (unsigned long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
