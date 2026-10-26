#pragma once
#include "IOConfig.h"
#include "IOText.h"
#include <string>

// SBQC's five-field arrays patch the native document; its other fields survive.
namespace IOProfile {
inline const char* nativeMode(const char* mode) {
    return !strcmp(mode, "OUT") ? "OUTPUT" : !strcmp(mode, "IN") ? "INPUT" :
        !strcmp(mode, "INPULL") ? "INPUT_PULLUP" : !strcmp(mode, "INPULLD") ? "INPUT_PULLDOWN" : nullptr;
}
inline const char* wireMode(const char* mode) {
    return !strcmp(mode, "OUTPUT") ? "OUT" : !strcmp(mode, "INPUT_PULLUP") ? "INPULL" :
        !strcmp(mode, "INPUT_PULLDOWN") ? "INPULLD" : "IN";
}
inline void describe(const JsonDocument& native, JsonArray result);
template<class Reserved>
bool merge(JsonDocument& document, const JsonDocument& incoming, Reserved reserved) {
    if (!incoming.is<JsonArrayConst>() || incoming.size() > 32 || measureJson(incoming) > 3800) return false;
    JsonDocument candidate = document;
    JsonArray pins = candidate["io_pins"].as<JsonArray>();
    if (pins.isNull()) return false;
    bool seen[40] = {};
    for (JsonVariantConst item : incoming.as<JsonArrayConst>()) {
        if (!item.is<JsonObjectConst>() || item.size() != 5 || !item["io"].is<int>() ||
            !IOText::bounded(item["mode"], 7) || !item["isA"].is<bool>() ||
            !IOText::bounded(item["lbl"], 64) || !IOText::bounded(item["name"], 64)) return false;
        int io = item["io"];
        if (!IOConfig::available(io) || seen[io] || reserved(io)) return false;
        seen[io] = true;
        const char* wire = item["mode"];
        const char* mode = nativeMode(wire);
        const bool analog = item["isA"];
        if (!mode || (IOConfig::inputOnly(io) && strcmp(wire, "IN")) ||
            (analog && (strcmp(wire, "IN") || !IOConfig::analog(io)))) return false;
        JsonObject entry;
        for (JsonObject pin : pins) if (pin["gpio"].as<int>() == io) { entry = pin; break; }
        if (entry.isNull()) entry = pins.add<JsonObject>();
        if (!strcmp(mode, "OUTPUT") && strcmp(entry["mode"] | "INPUT", "OUTPUT"))
            entry["initial_state"] = (entry["inverted"] | false) ? "HIGH" : "LOW";
        entry["gpio"] = io; entry["mode"] = mode;
        entry["lbl"] = item["lbl"]; entry["name"] = item["name"];
        const char* name = item["name"], *label = item["lbl"];
        if (*name || *label) entry["label"] = *name ? item["name"] : item["lbl"];
        else entry["label"] = std::string("GPIO ") + std::to_string(io);
        entry["type"] = analog ? "ANALOG_INPUT" : "DIGITAL";
    }
    if (pins.size() > 32 || IOConfig::validate(candidate)) return false;
    JsonDocument effective;
    describe(candidate, effective.to<JsonArray>());
    if (measureJson(effective) > 3800) return false;
    document = candidate;
    return true;
}
inline void describe(const JsonDocument& native, JsonArray result) {
    for (JsonVariantConst pin : native["io_pins"].as<JsonArrayConst>()) {
        JsonObject row = result.add<JsonObject>();
        row["io"] = pin["gpio"];
        // Input-only pins have effective INPUT even for a saved legacy pull mode.
        row["mode"] = IOConfig::inputOnly(pin["gpio"]) ? "IN" : wireMode(pin["mode"] | "INPUT");
        row["isA"] = !strcmp(pin["type"] | "DIGITAL", "ANALOG_INPUT");
        row["lbl"] = pin["lbl"].is<const char*>() ? pin["lbl"] : pin["label"];
        row["name"] = pin["name"] | "";
    }
}
}
