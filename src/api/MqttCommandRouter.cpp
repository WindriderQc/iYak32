#include "IOControl.h"
#include "MqttCommandRouter.h"
#include "Esp32.h"
#include "Mqtt.h"
#include <ArduinoJson.h>

namespace MqttCommandRouter {

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
        if (!Esp32::setPinActive(io, turnOn, 0, IOEvents::Source::MQTT)) {
            Serial.println(F("MQTT Error: GPIO is not a configured digital output."));
            return;
        }
        Serial.printf("Setting %s output %d\n", turnOn ? "ON" : "OFF", io);
    }

    void handleIncoming(char* topic, byte* message, unsigned int length) {
        Mqtt::recordIncoming(topic, message, length);
        String topicStr = String(topic);
        if (IOControl::receive(topicStr, message, length)) return;
        #ifdef VERBOSE
        Serial.println(topicStr);
        Serial.print("Msg bytes: "); Serial.println(length);
        #endif

        if (topicStr == "homeassistant/status") {
            if (length == 6 && memcmp(message, "online", 6) == 0) Mqtt::requestHomeAssistantDiscovery();
            return;
        }
        const String deviceTopic = String("esp32/") + Esp32::DEVICE_NAME;
        bool handled = true;
        if (topicStr == deviceTopic + "/automation/pause/set") {
            bool on = length == 2 && !memcmp(message, "ON", 2), off = length == 3 && !memcmp(message, "OFF", 3);
            if ((!on && !off) || !Esp32::setAutomationPaused(on)) Serial.println(F("MQTT Error: automation pause rejected."));
        } else if (topicStr == deviceTopic + "/io/override") {
            JsonDocument command;
            if (deserializeJson(command, message, length) || !command["gpio"].is<int>() || !command["duration_ms"].is<uint32_t>()) Serial.println(F("MQTT Error: override requires gpio and duration_ms."));
            else Serial.println(Esp32::setIOOverrideJson(command["gpio"], command["duration_ms"], IOEvents::Source::MQTT));
        } else if (topicStr == deviceTopic + "/io/on") {
            handleIOCommand(message, length, true);
        } else if (topicStr == deviceTopic + "/io/off") {
            handleIOCommand(message, length, false);
        } else if (topicStr == deviceTopic + "/io/pulse") {
            JsonDocument command;
            if (deserializeJson(command, message, length) || !command["gpio"].is<int>() ||
                !command["duration_ms"].is<uint32_t>() || command["duration_ms"].as<uint32_t>() == 0 ||
                !Esp32::setPinActive(command["gpio"], true, command["duration_ms"], IOEvents::Source::MQTT))
                Serial.println(F("MQTT Error: pulse requires a configured output and duration_ms from 100 to 3600000."));
        } else if (topicStr == deviceTopic + "/io/scene") {
            JsonDocument command;
            if (deserializeJson(command, message, length) || !command["id"].is<const char*>()) {
                Serial.println(F("MQTT Error: scene requires a JSON id."));
            } else {
                String result = Esp32::runIOSceneJson(command["id"].as<String>(), command["_revision"] | "", IOEvents::Source::MQTT);
                Serial.println(result);
            }
        } else if (topicStr.startsWith(deviceTopic + "/io/") && topicStr.endsWith("/set")) {
            String gpioText = topicStr.substring(deviceTopic.length() + 4, topicStr.length() - 4);
            int gpio = gpioText.toInt();
            bool on = length == 2 && memcmp(message, "ON", 2) == 0;
            bool off = length == 3 && memcmp(message, "OFF", 3) == 0;
            if (gpioText != String(gpio) || (!on && !off) || !Esp32::setPinActive(gpio, on, 0, IOEvents::Source::MQTT))
                Serial.println(F("MQTT Error: unavailable output or invalid ON/OFF command."));
        } else if (topicStr == deviceTopic + "/reboot") {
            Esp32::reboot();
        } else handled = false;
        if (handled) IOControl::changed();
    }

} // namespace MqttCommandRouter
