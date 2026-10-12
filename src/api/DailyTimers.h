#pragma once

#include <ctime>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>

// Daily GPIO alarms retain SBQC's sunrise/nightfall wire format. The device's
// native SNTP clock is authoritative; there is no second TimeLib clock.
namespace DailyTimers {
constexpr unsigned PINS = 40;
constexpr uint32_t PAIR_WINDOW_MS = 20000;
inline bool parseTime(const char* text, unsigned& seconds) {
    if (!text || std::strlen(text) != 8 || text[2] != ':' || text[5] != ':') return false;
    for (unsigned i : {0u, 1u, 3u, 4u, 6u, 7u}) if (text[i] < '0' || text[i] > '9') return false;
    unsigned h = (text[0]-'0')*10 + text[1]-'0', m = (text[3]-'0')*10 + text[4]-'0', s = (text[6]-'0')*10 + text[7]-'0';
    if (h > 23 || m > 59 || s > 59) return false;
    seconds = h*3600 + m*60 + s; return true;
}
inline bool parseMessage(const char* text, unsigned& gpio, unsigned& seconds) {
    if (!text) return false;
    unsigned n = 0; gpio = 0;
    while (text[n] && text[n] != ':') {
        if (n >= 2 || text[n] < '0' || text[n] > '9') return false;
        gpio = gpio*10 + text[n++] - '0';
    }
    return n && gpio < PINS && text[n] == ':' && parseTime(text+n+1, seconds);
}
inline void formatTime(unsigned seconds, char (&text)[9]) {
    std::snprintf(text, sizeof(text), "%02u:%02u:%02u", seconds/3600, seconds/60%60, seconds%60);
}
inline time_t nextStart(time_t now, unsigned seconds) {
    std::tm local{}; localtime_r(&now, &local);
    local.tm_hour = seconds/3600; local.tm_min = seconds/60%60; local.tm_sec = seconds%60; local.tm_isdst = -1;
    time_t result = std::mktime(&local);
    if (result <= now) { local.tm_mday++; local.tm_hour = seconds/3600; local.tm_min = seconds/60%60; local.tm_sec = seconds%60; local.tm_isdst = -1; result = std::mktime(&local); }
    return result;
}
inline time_t stopAfter(time_t start, unsigned on, unsigned off) {
    std::tm local{}; localtime_r(&start, &local);
    if (off < on) local.tm_mday++;
    local.tm_hour = off/3600; local.tm_min = off/60%60; local.tm_sec = off%60; local.tm_isdst = -1;
    return std::mktime(&local);
}
struct Schedule {
    unsigned on, off; bool enabled;
    Schedule(unsigned start = 21600, unsigned stop = 86399, bool active = false) : on(start), off(stop), enabled(active) {}
};
struct Pending {
    unsigned on = 0, off = 0; bool hasOn = false, hasOff = false; uint32_t at = 0;
    bool receive(bool sunrise, unsigned seconds, uint32_t now, Schedule& complete) {
        if ((!hasOn && !hasOff) || now-at >= PAIR_WINDOW_MS) { hasOn = hasOff = false; at = now; }
        if (sunrise) { on = seconds; hasOn = true; } else { off = seconds; hasOff = true; }
        if (!hasOn || !hasOff) return false;
        hasOn = hasOff = false;
        if (on == off) return false;
        complete = {on, off, true}; return true;
    }
    void cancel() { hasOn = hasOff = false; }
};
struct Runtime {
    time_t onAt = 0, offAt = 0, last = 0;
    bool active = false;
    void reset() { onAt = offAt = last = 0; active = false; }
    template<class Write>
    void tick(const Schedule& schedule, time_t now, bool clockValid, bool available, Write write) {
        if (!clockValid || !available || !schedule.enabled || (last && now < last)) {
            if (active && available) write(false);
            reset();
            if (!clockValid || !available || !schedule.enabled) return;
        }
        last = now;
        if (!onAt) { onAt = nextStart(now, schedule.on); offAt = stopAfter(onAt, schedule.on, schedule.off); return; }
        if (now >= offAt) {
            if (active) write(false);
            active = false; onAt = nextStart(now, schedule.on); offAt = stopAfter(onAt, schedule.on, schedule.off);
        } else if (!active && now >= onAt) active = write(true);
    }
};
} // namespace DailyTimers
