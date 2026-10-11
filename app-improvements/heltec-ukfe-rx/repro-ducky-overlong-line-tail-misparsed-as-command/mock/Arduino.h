// Schlanker Host-Mock fuer Arduino.h — nur die Teile, die ducky.h tatsaechlich benutzt
// (String-Klasse + delay()). Kein Hardware-/Timing-Verhalten, reine Logik-Verifikation.
#pragma once
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string>

class String {
public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(const String& o) : s_(o.s_) {}
    String& operator=(const String& o) { s_ = o.s_; return *this; }

    void toUpperCase() { for(char& c : s_) c = (char)toupper((unsigned char)c); }
    int length() const { return (int)s_.size(); }
    char operator[](int i) const { return s_[(size_t)i]; }
    bool operator==(const char* o) const { return s_ == o; }
    int indexOf(char c) const {
        size_t p = s_.find(c);
        return p == std::string::npos ? -1 : (int)p;
    }
    String substring(int start, int end = -1) const {
        int len = (int)s_.size();
        if(start < 0) start = 0;
        if(start > len) start = len;
        if(end < 0 || end > len) end = len;
        if(start > end) start = end;
        return String(s_.substr((size_t)start, (size_t)(end - start)).c_str());
    }
    long toInt() const { return strtol(s_.c_str(), nullptr, 10); }
    const char* c_str() const { return s_.c_str(); }

private:
    std::string s_;
};

inline void delay(uint32_t) {}
