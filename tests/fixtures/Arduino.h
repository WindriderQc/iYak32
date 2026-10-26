#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <ArduinoJson.h>
using byte = unsigned char;
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& text) : std::string(text) {}
    bool isEmpty() const { return empty(); }
    size_t write(uint8_t ch) { push_back(static_cast<char>(ch)); return 1; }
    size_t write(const uint8_t* bytes, size_t count) { append(reinterpret_cast<const char*>(bytes), count); return count; }
};
inline void convertToJson(const String& value, JsonVariant target) { target.set(std::string(value)); }
namespace IOTest { inline uint32_t milliseconds = 1000; }
inline uint32_t millis() { return IOTest::milliseconds; }
inline uint32_t esp_random() { return 0x1234abcd; }
