#include "JsonTools.h"
#include <Arduino.h> // For Serial (if used for debugging inside, though not in current plan)

namespace JsonTools {
    String getJsonString(const JsonDocument& doc, bool isPretty) {
        String str = ""; // Initialize to empty string
        if (doc.isNull()) {
            // Return "null" for a null document, or "{}" for an empty object if that's preferred.
            return "null";
        }
        if (doc.overflowed()) {
            // Handle overflow: log error and return an error indicator or empty string.
            // This check is more relevant if 'doc' was the result of a deserialization
            // that might have overflowed a StaticJsonDocument. For serialization,
            // serializeJson itself will return 0 if it can't write to the output buffer.
            // Serial.println(F("JsonTools Error: Source JsonDocument is overflowed."));
            // return "{\"error\":\"source_overflow\"}";
        }
        if (isPretty) {
            serializeJsonPretty(doc, str);
        } else {
            serializeJson(doc, str);
        }
        // serializeJson returns 0 on error (e.g. output buffer too small, though String grows)
        // However, for String output, it's less likely to fail due to buffer size unless memory is exhausted.
        // The `str` will simply contain what could be serialized.
        return str;
    }
}
