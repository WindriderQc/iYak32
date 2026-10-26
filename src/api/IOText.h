#pragma once
#include <ArduinoJson.h>
#include <cstdint>

// Match Data's UTF-16 string limit, without accepting control/NUL characters.
namespace IOText {
inline bool bounded(JsonVariantConst value, size_t limit) {
    if (!value.is<const char*>()) return false;
    JsonString text = value.as<JsonString>();
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.c_str());
    size_t units = 0;
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = bytes[i++]; unsigned tails = 0; uint32_t minimum = 0;
        if (cp < 0x80) { if (cp < 32 || cp == 127) return false; }
        else if (cp >= 0xc2 && cp <= 0xdf) { cp &= 0x1f; tails = 1; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { cp &= 0x0f; tails = 2; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; tails = 3; minimum = 0x10000; }
        else return false;
        if (i + tails > text.size()) return false;
        for (unsigned n = 0; n < tails; ++n) {
            if ((bytes[i] & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (bytes[i++] & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        units += cp > 0xffff ? 2 : 1;
        if (units > limit) return false;
    }
    return true;
}
}
