#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdint>

namespace IOAlerts {
enum class Change { NONE, ACTIVE, CLEARED };
struct State {
    bool active = false, candidate = false, initialized = false;
    uint32_t candidateAt = 0, changedAt = 0;
    void unavailable() { active = false; initialized = false; }
};
// Alerts may report an initially exceeded threshold after stability. They never command a GPIO.
inline Change evaluate(JsonVariantConst alert, State& state, double value, uint32_t now) {
    if (!std::isfinite(value)) { state.unavailable(); return Change::NONE; }
    const bool above = !std::strcmp(alert["operator"] | "above", "above");
    const double threshold = alert["threshold"], margin = alert["hysteresis"];
    bool current = above ? value > threshold - (state.active ? margin : 0) : value < threshold + (state.active ? margin : 0);
    if (!state.initialized || current != state.candidate) { state.initialized = true; state.candidate = current; state.candidateAt = now; }
    if (current == state.active || now - state.candidateAt < (alert["stable_ms"] | 2000U)) return Change::NONE;
    state.active = current; state.changedAt = now; return current ? Change::ACTIVE : Change::CLEARED;
}
inline const char* validate(const JsonDocument& doc) {
    auto alerts = doc["alerts"];
    if (alerts.isNull()) return nullptr;
    if (!alerts.is<JsonArrayConst>() || alerts.size() > 12) return "Configurez au maximum douze alertes.";
    const char* ids[12] = {}; size_t count = 0;
    for (JsonVariantConst alert : alerts.as<JsonArrayConst>()) {
        const char* id = alert["id"] | "", *name = alert["name"] | "";
        if (!alert.is<JsonObjectConst>() || !*id || std::strlen(id) > 32 || !alert["enabled"].is<bool>()) return "Alerte : identifiant et état activé requis.";
        for (const char* p = id; *p; ++p) if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '_' && *p != '-') return "Identifiant d’alerte invalide.";
        for (size_t i = 0; i < count; ++i) if (!std::strcmp(ids[i], id)) return "Les identifiants d’alerte doivent être uniques.";
        ids[count++] = id;
        while (*name && std::isspace(static_cast<unsigned char>(*name))) ++name;
        if (!alert["name"].is<const char*>() || !*name || std::strlen(alert["name"].as<const char*>()) > 64) return "Donnez un nom de 1 à 64 caractères à chaque alerte.";
        const char* source = alert["source"] | "";
        if (!std::strcmp(source, "gpio")) {
            bool found = false;
            if (!alert["gpio"].is<int>()) return "Choisissez une entrée analogique pour l’alerte.";
            for (JsonVariantConst pin : doc["io_pins"].as<JsonArrayConst>())
                if (pin["gpio"].as<int>() == alert["gpio"].as<int>() && !std::strcmp(pin["type"] | "DIGITAL", "ANALOG_INPUT") && std::strcmp(pin["mode"] | "INPUT", "OUTPUT")) found = true;
            if (!found) return "L’alerte doit cibler une entrée analogique configurée. Modifiez aussi ses références lors d’un retrait.";
        } else if (std::strcmp(source, "temperature")) return "Source d’alerte : température ou entrée analogique.";
        const char* op = alert["operator"] | "";
        if (std::strcmp(op, "above") && std::strcmp(op, "below")) return "Alerte : choisissez au-dessus ou sous le seuil.";
        for (const char* key : {"threshold", "hysteresis"})
            if (!alert[key].is<double>() || !std::isfinite(alert[key].as<double>()) || std::fabs(alert[key].as<double>()) > 1e9) return "Seuil et marge d’alerte doivent être des nombres finis.";
        if (alert["hysteresis"].as<double>() < 0 || !alert["stable_ms"].is<uint32_t>() || alert["stable_ms"].as<uint32_t>() < 500 || alert["stable_ms"].as<uint32_t>() > 60000) return "Marge positive ou nulle ; stabilité d’alerte de 0,5 à 60 s.";
    }
    return nullptr;
}
}
