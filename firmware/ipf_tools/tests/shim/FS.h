/* Minimal fs::FS shim over stdio (tests only). Paths are relative to root. */
#pragma once
#include "Arduino.h"
#include <sys/stat.h>
#include <unistd.h>

#define FILE_READ "r"
#define FILE_WRITE "w"

class File {
public:
    FILE *f = nullptr;
    File() {}
    explicit File(FILE *x) : f(x) {}
    explicit operator bool() const { return f != nullptr; }
    uint32_t size() { long p = ftell(f); fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, p, SEEK_SET); return (uint32_t)s; }
    uint32_t position() { return (uint32_t)ftell(f); }
    bool seek(uint32_t p) { return fseek(f, (long)p, SEEK_SET) == 0; }
    int read(uint8_t *b, size_t n) { return (int)fread(b, 1, n, f); }
    size_t write(const uint8_t *b, size_t n) { return fwrite(b, 1, n, f); }
    void close() { if (f) fclose(f); f = nullptr; }
};

namespace fs {
class FS {
public:
    std::string root;
    explicit FS(const char *r) : root(r) {}
    std::string p(const char *x) { return root + x; }
    File open(const char *x, const char *m) { return File(fopen(p(x).c_str(), m[0] == 'w' ? "w+b" : "rb")); }
    File open(const String &x, const char *m) { return open(x.c_str(), m); }
    bool exists(const char *x) { struct stat st; return stat(p(x).c_str(), &st) == 0; }
    bool exists(const String &x) { return exists(x.c_str()); }
    bool remove(const char *x) { return ::remove(p(x).c_str()) == 0; }
    bool remove(const String &x) { return remove(x.c_str()); }
    bool mkdir(const char *x) { return ::mkdir(p(x).c_str(), 0755) == 0; }
    bool rename(const String &a, const String &b) { return ::rename(p(a.c_str()).c_str(), p(b.c_str()).c_str()) == 0; }
};
}
