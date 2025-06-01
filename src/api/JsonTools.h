#pragma once
#include <ArduinoJson.h> // For JsonDocument (which includes Arduino.h for String on ESP32)

namespace JsonTools {
    // Takes JsonDocument by const reference
    String getJsonString(const JsonDocument& doc, bool isPretty = false);
}
