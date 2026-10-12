#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace IOControl {
void begin();
void tick();
bool receive(const String& topic, const byte* message, unsigned length);
void changed();
bool loadBootIOConfiguration(JsonDocument& doc);
} // namespace IOControl
