#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include <string>
#include "IOOperator.h"

namespace IODiscovery {
inline const char* domain(const char* mode, const char* type) {
    if (!std::strcmp(type, "ANALOG_INPUT")) return "sensor";
    return !std::strcmp(mode, "OUTPUT") ? "switch" : "binary_sensor";
}
inline void build(JsonDocument& doc, const char* device, int gpio, const char* label,
                  const char* component, const char* configUrl, JsonVariantConst calibration = JsonVariantConst(),
                  const char* friendly = nullptr, const char* icon = "") {
    doc.clear();
    const std::string identity = std::string(device) + "_gpio" + std::to_string(gpio);
    const std::string prefix = std::string("esp32/") + device + "/io/" + std::to_string(gpio);
    doc["unique_id"] = identity; doc["name"] = label;
    doc["state_topic"] = prefix + "/state";
    JsonObject parent = doc["device"].to<JsonObject>();
    parent["identifiers"].to<JsonArray>().add(device);
    parent["name"] = friendly && *friendly ? friendly : device; parent["manufacturer"] = "iYak32";
    parent["configuration_url"] = configUrl;
    doc["origin"]["name"] = "iYak32";
    JsonArray availability = doc["availability"].to<JsonArray>();
    availability.add<JsonObject>()["topic"] = std::string("sensors/") + device + "/availability";
    JsonObject pinAvailable = availability.add<JsonObject>();
    pinAvailable["topic"] = prefix + "/state";
    pinAvailable["value_template"] = "{{ 'online' if value_json.value is number else 'offline' }}";
    doc["availability_mode"] = "all";
    if (!std::strcmp(component, "switch")) {
        doc["command_topic"] = prefix + "/set";
        doc["payload_on"] = "ON"; doc["payload_off"] = "OFF";
        doc["value_template"] = "{{ 'ON' if value_json.active else 'OFF' }}";
        doc["optimistic"] = false; doc["retain"] = false;
        doc["icon"] = "mdi:toggle-switch";
    } else if (!std::strcmp(component, "binary_sensor")) {
        doc["payload_on"] = "HIGH"; doc["payload_off"] = "LOW";
        doc["value_template"] = "{{ 'HIGH' if value_json.value == 1 else 'LOW' }}";
        doc["icon"] = "mdi:electric-switch";
    } else {
        doc["value_template"] = calibration.isNull() ? "{{ value_json.value }}" : "{{ value_json.calibrated_value }}";
        const char* unit = calibration.isNull() ? "ADC" : (calibration["unit"] | "");
        if (*unit) doc["unit_of_measurement"] = unit;
        doc["state_class"] = "measurement";
        doc["icon"] = "mdi:gauge";
    }
    doc["json_attributes_topic"] = prefix + "/state";
    doc["json_attributes_template"] = "{{ {'gpio': value_json.gpio, 'level': value_json.value, 'remaining_ms': value_json.remaining_ms | default(0)} | tojson }}";
    if (const char* chosen = IOOperator::mdi(icon)) doc["icon"] = chosen;
}
inline void control(JsonDocument& doc, const char* device, const char* friendly, const char* url) {
    doc.clear(); doc["device"]["identifiers"].to<JsonArray>().add(device);
    doc["device"]["name"] = friendly; doc["device"]["manufacturer"] = "iYak32";
    doc["device"]["configuration_url"] = url; doc["origin"]["name"] = "iYak32";
    doc["availability_topic"] = std::string("sensors/") + device + "/availability";
    doc["retain"] = false;
}
inline void scene(JsonDocument& doc, const char* device, const char* friendly, const char* url, JsonVariantConst definition) {
    control(doc, device, friendly, url);
    const char* id = definition["id"] | "all_off";
    doc["unique_id"] = std::string(device) + "_scene_" + id;
    doc["name"] = definition["name"] | "Tout OFF";
    doc["command_topic"] = std::string("esp32/") + device + "/io/scene";
    JsonDocument command; command["id"] = id; std::string payload; serializeJson(command, payload); doc["payload_press"] = payload;
    const char* chosen = IOOperator::mdi(definition["icon"] | "scene"); doc["icon"] = chosen ? chosen : "mdi:play-circle";
    if (std::strcmp(id, "all_off")) {
        doc.remove("availability_topic"); JsonArray availability = doc["availability"].to<JsonArray>();
        availability.add<JsonObject>()["topic"] = std::string("sensors/") + device + "/availability";
        JsonObject sceneAvailable = availability.add<JsonObject>();
        sceneAvailable["topic"] = std::string("esp32/") + device + "/io/scene/" + id + "/state";
        sceneAvailable["value_template"] = "{{ 'online' if value_json.available else 'offline' }}";
        doc["availability_mode"] = "all";
    }
}
inline void pause(JsonDocument& doc, const char* device, const char* friendly, const char* url) {
    control(doc, device, friendly, url);
    doc["unique_id"] = std::string(device) + "_automation_pause"; doc["name"] = "Pause des automatismes";
    doc["command_topic"] = std::string("esp32/") + device + "/automation/pause/set";
    doc["state_topic"] = std::string("esp32/") + device + "/automation/state";
    doc["value_template"] = "{{ 'ON' if value_json.paused else 'OFF' }}";
    doc["payload_on"] = "ON"; doc["payload_off"] = "OFF"; doc["optimistic"] = false; doc["icon"] = "mdi:pause-circle";
}
}
