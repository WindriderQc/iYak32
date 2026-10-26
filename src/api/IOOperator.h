#pragma once
#include "IOOutput.h"
#include "IOScenes.h"
#include <cstring>

namespace IOOperator {
constexpr uint32_t MAX_OVERRIDE_MS = 86400000;
inline bool validOverride(uint32_t ms) { return ms == 0 || (ms >= 1000 && ms <= MAX_OVERRIDE_MS); }
inline const char* mdi(const char* icon) {
    if (!std::strcmp(icon, "light")) return "mdi:lightbulb";
    if (!std::strcmp(icon, "fan")) return "mdi:fan";
    if (!std::strcmp(icon, "water")) return "mdi:water-pump";
    if (!std::strcmp(icon, "plant")) return "mdi:sprout";
    if (!std::strcmp(icon, "temperature")) return "mdi:thermometer";
    if (!std::strcmp(icon, "button")) return "mdi:gesture-tap-button";
    if (!std::strcmp(icon, "sensor")) return "mdi:gauge";
    if (!std::strcmp(icon, "home")) return "mdi:home";
    if (!std::strcmp(icon, "scene")) return "mdi:play-circle";
    if (!std::strcmp(icon, "switch")) return "mdi:toggle-switch";
    return nullptr;
}
inline const char* presentation(JsonVariantConst object) {
    if (!object["icon"].isNull() && (!object["icon"].is<const char*>() || (*object["icon"].as<const char*>() && !mdi(object["icon"])))) return "Choisissez une icône disponible.";
    return nullptr;
}
inline const char* validateDevice(JsonVariantConst device) {
    if (device.isNull()) return nullptr;
    const char* name = device["name"] | "";
    if (!device.is<JsonObjectConst>() || !device["name"].is<const char*>() || !*name || std::strlen(name) > 64) return "Nom du board : de 1 à 64 caractères.";
    bool visible = false; for (const char* p = name; *p; ++p) if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') visible = true;
    return visible ? presentation(device) : "Donnez un nom au board.";
}
template<typename Available>
const char* zonePlan(const JsonDocument& config, const char* zone, Available available,
                     std::vector<IOScenes::Action>& actions, std::vector<int>& skipped) {
    actions.clear(); skipped.clear();
    if (!zone || !*zone || std::strlen(zone) > 64) return "Choisissez une zone nommée.";
    for (JsonVariantConst pin : config["io_pins"].as<JsonArrayConst>()) {
        if (std::strcmp(pin["zone"] | "", zone) || std::strcmp(pin["mode"] | "INPUT", "OUTPUT") || std::strcmp(pin["type"] | "DIGITAL", "DIGITAL")) continue;
        int gpio = pin["gpio"];
        if (available(gpio)) actions.push_back({gpio, false, 0}); else skipped.push_back(gpio);
    }
    return actions.empty() ? "Aucune sortie disponible dans cette zone." : nullptr;
}
template<typename Remaining>
uint32_t sceneHold(const JsonDocument& config, const char* id, Remaining remaining) {
    uint32_t hold = 0;
    if (!std::strcmp(id, "all_off")) {
        for (JsonVariantConst pin : config["io_pins"].as<JsonArrayConst>())
            if (!std::strcmp(pin["mode"] | "INPUT", "OUTPUT")) { uint32_t ms = remaining(pin["gpio"].as<int>()); if (ms > hold) hold = ms; }
    } else for (JsonVariantConst scene : config["scenes"].as<JsonArrayConst>()) if (!std::strcmp(scene["id"] | "", id))
        for (JsonVariantConst action : scene["actions"].as<JsonArrayConst>()) { uint32_t ms = remaining(action["gpio"].as<int>()); if (ms > hold) hold = ms; }
    return hold;
}
}
