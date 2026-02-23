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

    static void handleIOCommand(char* topic, byte* message, unsigned int length, const String& topicStr) {
        Mqtt::MqttMsg mqttMsg = Mqtt::sliceMqttMsg(topic, message, length);
        if (mqttMsg.msgTokens[0] == nullptr) {
            Serial.println(F("MQTT Error: Empty message payload, no IO pin specified."));
            return;
        }
        int io = atoi(mqttMsg.msgTokens[0]);
        if (Esp32::validIO(io)) {
            if (topicStr.indexOf("/on") >= 0) {
                digitalWrite(io, HIGH);
                Serial.print(F("Setting ON output ")); Serial.println(io);
            } else if (topicStr.indexOf("/off") >= 0) {
                digitalWrite(io, LOW);
                Serial.print(F("Setting OFF output ")); Serial.println(io);
            }
        } else {
            Serial.println(F("Invalid IO"));
        }
    }

    void handleIncoming(char* topic, byte* message, unsigned int length) {
        String topicStr = String(topic);
        #ifdef VERBOSE
        Serial.println(topicStr);
        Serial.print("Msg bytes: "); Serial.println(length);
        #endif

        if (topicStr.indexOf(Esp32::DEVICE_NAME) < 0) return;

        if (topicStr.indexOf("/configIOs") >= 0) {
            handleConfigIOs(message, length);
        } else if (topicStr.indexOf("/io/") >= 0) {
            handleIOCommand(topic, message, length, topicStr);
        } else if (topicStr.indexOf("/reboot") >= 0) {
            Esp32::reboot();
        }
    }

} // namespace MqttCommandRouter
