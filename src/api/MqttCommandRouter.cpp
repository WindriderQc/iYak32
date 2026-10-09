#include "MqttCommandRouter.h"
#include "Esp32.h"
#include "Mqtt.h"
#include <ArduinoJson.h>

namespace MqttCommandRouter {

    static void handleConfigIOs(byte* message, unsigned int length) {
        JsonDocument config;
        DeserializationError error = deserializeJson(config, message, length);
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }
        for (JsonObject elem : config.as<JsonArray>()) {
            unsigned int io = elem["io"];
            const char* mode = elem["mode"];
            const char* lbl = elem["lbl"];
            bool isA = elem["isA"];
            Esp32::configPin(io, mode, lbl, isA);
        }
        Serial.println(F("IO Config received and completed"));
    }

    static void handleIOCommand(byte* message, unsigned int length, bool turnOn) {
        // The first payload field is a decimal GPIO number; later fields are ignored.
        char pinText[3] = {};
        unsigned int digits = 0;
        while (digits < length && message[digits] != ':') {
            if (digits >= 2 || message[digits] < '0' || message[digits] > '9') {
                Serial.println(F("MQTT Error: Invalid GPIO number."));
                return;
            }
            pinText[digits] = static_cast<char>(message[digits]);
            ++digits;
        }
        if (digits == 0) {
            Serial.println(F("MQTT Error: Empty GPIO number."));
            return;
        }
        int io = atoi(pinText);
        if (!Esp32::validIO(io) || Esp32::ios[io]->config.pinMode != OUTPUT || Esp32::ios[io]->config.isAnalog) {
            Serial.println(F("MQTT Error: GPIO is not a configured digital output."));
            return;
        }
        digitalWrite(io, turnOn ? HIGH : LOW);
        Serial.printf("Setting %s output %d\n", turnOn ? "ON" : "OFF", io);
    }

    void handleIncoming(char* topic, byte* message, unsigned int length) {
        String topicStr = String(topic);
        #ifdef VERBOSE
        Serial.println(topicStr);
        Serial.print("Msg bytes: "); Serial.println(length);
        #endif

        const String deviceTopic = String("esp32/") + Esp32::DEVICE_NAME;
        if (topicStr == deviceTopic + "/configIOs") {
            handleConfigIOs(message, length);
        } else if (topicStr == deviceTopic + "/io/on") {
            handleIOCommand(message, length, true);
        } else if (topicStr == deviceTopic + "/io/off") {
            handleIOCommand(message, length, false);
        } else if (topicStr == deviceTopic + "/reboot") {
            Esp32::reboot();
        }
    }

} // namespace MqttCommandRouter
