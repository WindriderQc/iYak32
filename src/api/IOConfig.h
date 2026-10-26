#pragma once

#include <ArduinoJson.h>
#include <cctype>
#include <cstring>
#include "IOCalibration.h"
#include "IOOutput.h"
#include "IOAutomation.h"
#include "IOOperator.h"
#include "IOAlerts.h"
#include "IOHomeAssistant.h"

namespace IOConfig {
inline bool available(int gpio) {
    switch (gpio) {
        case 0: case 1: case 2: case 3: case 4: case 5:
        case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19:
        case 21: case 22: case 23: case 25: case 26: case 27:
        case 32: case 33: case 34: case 35: case 36: case 39: return true;
        default: return false;
    }
}
inline bool inputOnly(int gpio) { return gpio == 34 || gpio == 35 || gpio == 36 || gpio == 39; }
inline bool adc2(int gpio) {
    return gpio == 0 || gpio == 2 || gpio == 4 || gpio == 12 || gpio == 13 ||
           gpio == 14 || gpio == 15 || gpio == 25 || gpio == 26 || gpio == 27;
}
inline bool analog(int gpio) { return adc2(gpio) || inputOnly(gpio) || gpio == 32 || gpio == 33; }

// Legacy pull-up/down settings on input-only pins still load; status exposes the effective INPUT mode.
inline const char* validate(const JsonDocument& doc) {
    if (measureJson(doc) > 32768) return "La configuration dépasse 32 Ko.";
    if (!doc["io_pins"].is<JsonArrayConst>()) return "io_pins doit être une liste.";
    if (const char* error = IOHomeAssistant::validate(doc["home_assistant"])) return error;
    if (const char* error = IOOperator::validateDevice(doc["device"])) return error;
    bool seen[40] = {};
    for (JsonVariantConst entry : doc["io_pins"].as<JsonArrayConst>()) {
        if (!entry.is<JsonObjectConst>() || !entry["gpio"].is<int>()) return "Chaque broche doit avoir un GPIO entier.";
        int gpio = entry["gpio"].as<int>();
        if (const char* error = IOOperator::presentation(entry)) return error;
        if (!available(gpio)) return "GPIO indisponible sur cet ESP32.";
        if (seen[gpio]) return "Un GPIO ne peut être configuré qu'une fois.";
        seen[gpio] = true;
        const char* label = entry["label"] | "";
        while (*label && std::isspace(static_cast<unsigned char>(*label))) ++label;
        if (!*label) return "Donnez un nom à chaque broche configurée.";
        const char* mode = entry["mode"] | "INPUT";
        const char* type = entry["type"] | "DIGITAL";
        const bool output = std::strcmp(mode, "OUTPUT") == 0;
        if (!output && std::strcmp(mode, "INPUT") && std::strcmp(mode, "INPUT_PULLUP") && std::strcmp(mode, "INPUT_PULLDOWN"))
            return "Mode GPIO inconnu.";
        if (inputOnly(gpio) && output) return "GPIO 34, 35, 36 et 39 sont des entrées uniquement.";
        if (std::strcmp(type, "DIGITAL") && std::strcmp(type, "ANALOG_INPUT")) return "Type GPIO inconnu.";
        if (!std::strcmp(type, "ANALOG_INPUT") && (output || !analog(gpio))) return "L'analogique nécessite une entrée compatible ADC.";
        const char* initial = entry["initial_state"] | "";
        if (*initial && std::strcmp(initial, "LOW") && std::strcmp(initial, "HIGH")) return "État au démarrage inconnu.";
        if (!entry["inverted"].isNull() && !entry["inverted"].is<bool>()) return "La polarité inversée doit être un booléen.";
        if (!entry["label"].is<const char*>() || std::strlen(entry["label"].as<const char*>()) > 64) return "Le nom doit contenir de 1 à 64 caractères.";
        if (!entry["zone"].isNull() && (!entry["zone"].is<const char*>() || std::strlen(entry["zone"].as<const char*>()) > 64)) return "Une zone peut contenir jusqu'à 64 caractères.";
        if (!entry["pulse_presets_ms"].isNull()) {
            if (!output || !entry["pulse_presets_ms"].is<JsonArrayConst>() || entry["pulse_presets_ms"].size() > 5)
                return "Une sortie peut avoir jusqu'à cinq durées favorites.";
            uint32_t presets[5] = {}; size_t count = 0;
            for (JsonVariantConst preset : entry["pulse_presets_ms"].as<JsonArrayConst>()) {
                if (!preset.is<uint32_t>() || !IOOutput::validDuration(preset.as<uint32_t>())) return "Durée favorite : de 0,1 à 3600 secondes.";
                for (size_t i = 0; i < count; ++i) if (presets[i] == preset.as<uint32_t>()) return "Les durées favorites doivent être différentes.";
                presets[count++] = preset.as<uint32_t>();
            }
        }
        if (!entry["calibration"].isNull()) {
            if (std::strcmp(type, "ANALOG_INPUT")) return "La calibration nécessite une entrée analogique.";
            if (const char* error = IOCalibration::validate(entry["calibration"])) return error;
        }
    }
    if (!doc["scenes"].isNull()) {
        if (!doc["scenes"].is<JsonArrayConst>() || doc["scenes"].size() > 12) return "Configurez au maximum douze scènes.";
        const char* ids[12] = {}; size_t count = 0, totalActions = 0;
        for (JsonVariantConst scene : doc["scenes"].as<JsonArrayConst>()) {
            if (const char* error = IOOperator::presentation(scene)) return error;
            const char* id = scene["id"] | "";
            if (!scene.is<JsonObjectConst>() || !*id || std::strlen(id) > 32 || !std::strcmp(id, "all_off")) return "Identifiant de scène invalide.";
            for (const char* p = id; *p; ++p) if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '_' && *p != '-') return "Identifiant de scène invalide.";
            for (size_t i = 0; i < count; ++i) if (!std::strcmp(ids[i], id)) return "Les identifiants de scène doivent être uniques.";
            ids[count++] = id;
            const char* name = scene["name"] | "";
            if (!scene["zone"].isNull() && (!scene["zone"].is<const char*>() || std::strlen(scene["zone"].as<const char*>()) > 64)) return "Une zone peut contenir jusqu'à 64 caractères.";
            if (!scene["name"].is<const char*>() || !*name || std::strlen(name) > 64) return "Donnez un nom de 1 à 64 caractères à chaque scène.";
            while (*name && std::isspace(static_cast<unsigned char>(*name))) ++name;
            if (!*name) return "Donnez un nom à chaque scène.";
            if (!scene["actions"].is<JsonArrayConst>() || scene["actions"].size() == 0 || scene["actions"].size() > 26)
                return "Une scène doit commander de 1 à 26 sorties.";
            totalActions += scene["actions"].size();
            if (totalActions > 64) return "Les scènes peuvent contenir 64 actions au total.";
            bool used[40] = {};
            for (JsonVariantConst action : scene["actions"].as<JsonArrayConst>()) {
                if (!action.is<JsonObjectConst>() || !action["gpio"].is<int>() || !action["active"].is<bool>()) return "Chaque action nécessite un GPIO entier et un état ON/OFF.";
                int gpio = action["gpio"];
                if (!available(gpio) || used[gpio]) return "Une scène ne peut commander un GPIO qu'une fois.";
                used[gpio] = true;
                bool configured = false;
                for (JsonVariantConst pin : doc["io_pins"].as<JsonArrayConst>())
                    if (pin["gpio"].as<int>() == gpio && !std::strcmp(pin["mode"] | "INPUT", "OUTPUT") && !std::strcmp(pin["type"] | "DIGITAL", "DIGITAL")) configured = true;
                if (!configured) return "Chaque GPIO d'une scène doit être configuré en sortie numérique. Modifiez aussi la scène lors d'un retrait.";
                if (!action["duration_ms"].isNull() && (!action["duration_ms"].is<uint32_t>() ||
                    (action["duration_ms"].as<uint32_t>() && (!action["active"].as<bool>() || !IOOutput::validDuration(action["duration_ms"])))) )
                    return "Une durée de scène nécessite ON et de 0,1 à 3600 secondes.";
            }
        }
    }
    if (const char* error = IOAlerts::validate(doc)) return error;
    return IOAutomation::validate(doc);
}
} // namespace IOConfig
