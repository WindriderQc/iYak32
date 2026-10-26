#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <cstdint>

namespace IOAutomation {
inline bool clockValid(time_t epoch) { return epoch >= 1704067200; }
// Toronto changes at 07:00 UTC on March's second Sunday and 06:00 UTC on November's first Sunday.
inline int offset(time_t epoch, const char* zone) {
    if (!std::strcmp(zone, "UTC")) return 0;
    tm utc; gmtime_r(&epoch, &utc);
    bool dst = utc.tm_mon > 2 && utc.tm_mon < 10;
    if (utc.tm_mon == 2 || utc.tm_mon == 10) {
        int firstSunday = 1 + (7 - (utc.tm_wday - (utc.tm_mday - 1) % 7 + 7) % 7) % 7;
        int boundary = firstSunday + (utc.tm_mon == 2 ? 7 : 0);
        bool after = utc.tm_mday > boundary || (utc.tm_mday == boundary && utc.tm_hour >= (utc.tm_mon == 2 ? 7 : 6));
        dst = utc.tm_mon == 2 ? after : !after;
    }
    return (dst ? -4 : -5) * 3600;
}
inline tm calendar(time_t epoch, const char* zone) {
    time_t shifted = epoch + offset(epoch, zone); tm local; gmtime_r(&shifted, &local); return local;
}
inline bool validTime(const char* at) {
    return at && std::strlen(at) == 5 && at[2] == ':' && at[0] >= '0' && at[0] <= '2' && at[1] >= '0' && at[1] <= '9' && at[3] >= '0' && at[3] <= '5' && at[4] >= '0' && at[4] <= '9' && (at[0] != '2' || at[1] <= '3');
}
inline int minuteOf(const char* at) { return ((at[0] - '0') * 10 + at[1] - '0') * 60 + (at[3] - '0') * 10 + at[4] - '0'; }
inline bool windowAllowed(JsonVariantConst rule, time_t epoch, const char* zone) {
    JsonVariantConst window = rule["window"]; if (window.isNull()) return true;
    if (!clockValid(epoch)) return false;
    tm local = calendar(epoch, zone); int minute = local.tm_hour * 60 + local.tm_min;
    int start = minuteOf(window["start"]), end = minuteOf(window["end"]);
    return start < end ? minute >= start && minute < end : minute >= start || minute < end;
}
inline int64_t dayKey(const tm& local) { return (local.tm_year + 1900) * 1000LL + local.tm_yday; }
inline bool matches(JsonVariantConst schedule, const tm& local) {
    const char* at = schedule["time"] | "00:00";
    int hour = (at[0] - '0') * 10 + at[1] - '0', minute = (at[3] - '0') * 10 + at[4] - '0';
    if (hour != local.tm_hour || minute != local.tm_min) return false;
    for (int day : schedule["days"].as<JsonArrayConst>()) if (day == local.tm_wday) return true;
    return false;
}
inline time_t next(JsonVariantConst schedule, time_t now, const char* zone, int64_t firedDay = -1) {
    time_t localNow = now + offset(now, zone), midnight = localNow / 86400 * 86400;
    const char* at = schedule["time"] | "00:00";
    int seconds = ((at[0] - '0') * 10 + at[1] - '0') * 3600 + ((at[3] - '0') * 10 + at[4] - '0') * 60;
    for (int d = 0; d <= 7; ++d) {
        time_t wall = midnight + d * 86400 + seconds;
        // Try both offsets. A skipped spring hour has no matching instant; a repeated hour runs once.
        for (int hours : {4, 5}) {
            time_t candidate = wall + (!std::strcmp(zone, "UTC") ? 0 : hours * 3600);
            tm local = calendar(candidate, zone);
            if (candidate <= now || dayKey(local) == firedDay || !matches(schedule, local)) continue;
            time_t normalized = candidate + offset(candidate, zone);
            if (normalized != wall) continue;
            return candidate;
        }
    }
    return 0;
}
struct ScheduleState { int64_t firedDay = -1; time_t lastMinute = 0; };
inline bool due(JsonVariantConst schedule, ScheduleState& state, time_t now, const char* zone) {
    if (!clockValid(now)) return false;
    time_t minute = now / 60;
    if (!state.lastMinute) { state.lastMinute = minute; return false; } // No boot/save/resume catch-up.
    if (minute <= state.lastMinute) return false;
    state.lastMinute = minute;
    tm local = calendar(now, zone);
    if (dayKey(local) <= state.firedDay || !matches(schedule, local)) return false;
    state.firedDay = dayKey(local); return true;
}
struct RuleState {
    bool initialized = false, condition = false, candidate = false, fired = false;
    uint32_t candidateAt = 0, lastRun = 0;
    void unavailable() { initialized = false; }
};
inline bool condition(JsonVariantConst rule, double value, bool previous) {
    const char* op = rule["operator"] | "below";
    if (!std::strcmp(op, "high")) return value == 1;
    if (!std::strcmp(op, "low")) return value == 0;
    double threshold = rule["threshold"], margin = rule["hysteresis"] | 0.0;
    return !std::strcmp(op, "below") ? (previous ? value < threshold + margin : value < threshold)
                                     : (previous ? value > threshold - margin : value > threshold);
}
inline bool evaluate(JsonVariantConst rule, RuleState& state, double value, uint32_t now) {
    if (!std::isfinite(value)) { state.unavailable(); return false; }
    bool current = condition(rule, value, state.condition);
    if (!state.initialized) {
        state.initialized = true; state.condition = current; state.candidate = current; state.candidateAt = now;
        return false; // A true initial reading does not start an actuator.
    }
    if (current != state.candidate) { state.candidate = current; state.candidateAt = now; }
    if (current == state.condition || now - state.candidateAt < (rule["stable_ms"] | 2000U)) return false;
    // A rising edge during cooldown is consumed, never deferred into an unexpected later action.
    state.condition = current;
    if (!current || (state.fired && now - state.lastRun < (rule["cooldown_ms"] | 60000U))) return false;
    state.fired = true; state.lastRun = now; return true;
}
enum class Attempt { NONE, FIRE, OUTSIDE_WINDOW, CLOCK_UNAVAILABLE };
inline Attempt attempt(JsonVariantConst rule, RuleState& state, double value, uint32_t now, time_t epoch, const char* zone) {
    if (!evaluate(rule, state, value, now)) return Attempt::NONE;
    if (windowAllowed(rule, epoch, zone)) return Attempt::FIRE;
    return clockValid(epoch) ? Attempt::OUTSIDE_WINDOW : Attempt::CLOCK_UNAVAILABLE;
}
inline const char* validate(const JsonDocument& doc) {
    JsonVariantConst a = doc["automation"];
    if (a.isNull()) return nullptr;
    if (!a.is<JsonObjectConst>() || !a["paused"].is<bool>()) return "Les automatismes nécessitent un état pause explicite.";
    const char* zone = a["timezone"] | "";
    if (std::strcmp(zone, "America/Toronto") && std::strcmp(zone, "UTC")) return "Fuseau disponible : America/Toronto ou UTC.";
    const char* ids[24] = {}; size_t count = 0;
    for (const char* collection : {"schedules", "rules"}) {
        if (!a[collection].is<JsonArrayConst>() || a[collection].size() > 12) return "Configurez au maximum douze horaires et douze règles.";
        for (JsonVariantConst item : a[collection].as<JsonArrayConst>()) {
            const char* id = item["id"] | "", *name = item["name"] | "";
            if (!item.is<JsonObjectConst>() || !*id || std::strlen(id) > 32 || !*name || std::strlen(name) > 64 || !item["enabled"].is<bool>()) return "Automatisme : identifiant, nom et état activé requis.";
            for (const char* p = id; *p; ++p) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return "Identifiant d'automatisme invalide.";
            for (size_t i = 0; i < count; ++i) if (!std::strcmp(ids[i], id)) return "Identifiants d'automatisme dupliqués.";
            ids[count++] = id;
            const char* target = item["scene_id"] | "";
            JsonVariantConst scene;
            for (JsonVariantConst s : doc["scenes"].as<JsonArrayConst>()) if (!std::strcmp(s["id"] | "", target)) scene = s;
            if (std::strcmp(target, "all_off") && scene.isNull()) return "Chaque automatisme doit cibler une scène existante. Modifiez aussi ses références lors d'un retrait.";
            if (!std::strcmp(collection, "schedules")) {
                const char* at = item["time"] | "";
                if (std::strlen(at) != 5 || at[2] != ':' || at[0] < '0' || at[0] > '2' || at[1] < '0' || at[1] > '9' || at[3] < '0' || at[3] > '5' || at[4] < '0' || at[4] > '9' || (at[0] == '2' && at[1] > '3')) return "Horaire : heure valide HH:MM requise.";
                if (!item["days"].is<JsonArrayConst>() || !item["days"].size() || item["days"].size() > 7) return "Choisissez au moins un jour.";
                bool days[7] = {};
                for (JsonVariantConst day : item["days"].as<JsonArrayConst>()) { if (!day.is<int>() || day.as<int>() < 0 || day.as<int>() > 6 || days[day.as<int>()]) return "Jours invalides ou dupliqués."; days[day.as<int>()] = true; }
            } else {
                JsonVariantConst window = item["window"];
                if (!window.isNull() && (!window.is<JsonObjectConst>() || !window["start"].is<const char*>() || !window["end"].is<const char*>() || !validTime(window["start"]) || !validTime(window["end"]) || !std::strcmp(window["start"].as<const char*>(), window["end"].as<const char*>()))) return "Plage de règle : heures HH:MM valides et différentes requises.";
                if (!item["gpio"].is<int>()) return "Règle : une entrée configurée est requise.";
                JsonVariantConst pin;
                for (JsonVariantConst p : doc["io_pins"].as<JsonArrayConst>()) if (p["gpio"].as<int>() == item["gpio"].as<int>()) pin = p;
                if (pin.isNull() || !std::strcmp(pin["mode"] | "INPUT", "OUTPUT")) return "La source d'une règle doit être une entrée configurée.";
                const char* op = item["operator"] | "";
                bool analog = !std::strcmp(pin["type"] | "DIGITAL", "ANALOG_INPUT");
                if (analog ? (std::strcmp(op, "below") && std::strcmp(op, "above")) : (std::strcmp(op, "high") && std::strcmp(op, "low"))) return "Condition incompatible avec le type d'entrée.";
                if (analog && (!item["threshold"].is<double>() || !std::isfinite(item["threshold"].as<double>()) || std::fabs(item["threshold"].as<double>()) > 1e9 || !item["hysteresis"].is<double>() || !std::isfinite(item["hysteresis"].as<double>()) || item["hysteresis"].as<double>() < 0 || item["hysteresis"].as<double>() > 1e9)) return "Seuil et marge doivent être des nombres finis, marge positive ou nulle.";
                if (!item["stable_ms"].is<uint32_t>() || item["stable_ms"].as<uint32_t>() < 500 || item["stable_ms"].as<uint32_t>() > 60000 || !item["cooldown_ms"].is<uint32_t>() || item["cooldown_ms"].as<uint32_t>() < 1000 || item["cooldown_ms"].as<uint32_t>() > 86400000) return "Stabilité : 0,5–60 s. Délai entre déclenchements : 1–86400 s.";
                for (JsonVariantConst action : scene["actions"].as<JsonArrayConst>()) if (action["active"].as<bool>() && !action["duration_ms"].as<uint32_t>()) return "Une règle nécessite des actions ON avec une durée. Réglez les durées dans la scène.";
            }
        }
    }
    return nullptr;
}
}
