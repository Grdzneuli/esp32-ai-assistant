#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#ifdef NATIVE_BUILD

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>

// Arduino types
typedef uint8_t byte;
typedef bool boolean;

// String class mock
class String {
public:
    std::string _str;

    String() : _str("") {}
    String(const char* s) : _str(s ? s : "") {}
    String(const std::string& s) : _str(s) {}
    String(int val) : _str(std::to_string(val)) {}
    String(unsigned int val) : _str(std::to_string(val)) {}
    String(long val) : _str(std::to_string(val)) {}
    String(unsigned long val) : _str(std::to_string(val)) {}
    String(float val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        _str = buf;
    }

    const char* c_str() const { return _str.c_str(); }
    size_t length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }

    String& operator=(const char* s) { _str = s ? s : ""; return *this; }
    String& operator=(const String& s) { _str = s._str; return *this; }

    String operator+(const String& s) const { return String(_str + s._str); }
    String operator+(const char* s) const { return String(_str + (s ? s : "")); }
    String& operator+=(const String& s) { _str += s._str; return *this; }
    String& operator+=(const char* s) { if (s) _str += s; return *this; }
    String& operator+=(char c) { _str += c; return *this; }

    bool operator==(const String& s) const { return _str == s._str; }
    bool operator==(const char* s) const { return _str == (s ? s : ""); }
    bool operator!=(const String& s) const { return _str != s._str; }

    char operator[](size_t i) const { return _str[i]; }
    char& operator[](size_t i) { return _str[i]; }

    int indexOf(char c, size_t from = 0) const {
        size_t pos = _str.find(c, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    int indexOf(const String& s, size_t from = 0) const {
        size_t pos = _str.find(s._str, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    String substring(size_t from, size_t to = std::string::npos) const {
        if (from >= _str.length()) return String();
        if (to == std::string::npos) to = _str.length();
        return String(_str.substr(from, to - from));
    }

    void trim() {
        size_t start = _str.find_first_not_of(" \t\n\r");
        size_t end = _str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) _str = "";
        else _str = _str.substr(start, end - start + 1);
    }

    void toLowerCase() {
        std::transform(_str.begin(), _str.end(), _str.begin(), ::tolower);
    }

    void toUpperCase() {
        std::transform(_str.begin(), _str.end(), _str.begin(), ::toupper);
    }

    bool startsWith(const String& prefix) const {
        return _str.substr(0, prefix.length()) == prefix._str;
    }

    bool endsWith(const String& suffix) const {
        if (suffix.length() > _str.length()) return false;
        return _str.substr(_str.length() - suffix.length()) == suffix._str;
    }

    void replace(const String& from, const String& to) {
        size_t pos = 0;
        while ((pos = _str.find(from._str, pos)) != std::string::npos) {
            _str.replace(pos, from.length(), to._str);
            pos += to.length();
        }
    }

    int toInt() const { return atoi(_str.c_str()); }
    float toFloat() const { return atof(_str.c_str()); }
};

inline String operator+(const char* lhs, const String& rhs) {
    return String(lhs) + rhs;
}

// Arduino functions
inline int constrain(int val, int min, int max) {
    return val < min ? min : (val > max ? max : val);
}

inline float constrain(float val, float min, float max) {
    return val < min ? min : (val > max ? max : val);
}

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }

template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Timing (mock)
inline unsigned long millis() {
    static unsigned long ms = 0;
    return ms++;
}

inline void delay(unsigned long ms) {
    (void)ms;  // No-op in tests
}

// Serial mock
class MockSerial {
public:
    void begin(unsigned long) {}
    void print(const char*) {}
    void print(const String& s) { (void)s; }
    void print(int) {}
    void println() {}
    void println(const char*) {}
    void println(const String& s) { (void)s; }
    void println(int) {}
    int printf(const char*, ...) { return 0; }
};

extern MockSerial Serial;

// PSRAM mock
inline bool psramFound() { return false; }
inline void* ps_malloc(size_t size) { return malloc(size); }

#endif // NATIVE_BUILD

#endif // MOCK_ARDUINO_H
