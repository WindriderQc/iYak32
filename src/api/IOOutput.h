#pragma once
#include <cstdint>

namespace IOOutput {
inline bool level(bool active, bool inverted) { return active != inverted; }
inline bool active(bool high, bool inverted) { return high != inverted; }
constexpr uint32_t MAX_PULSE_MS = 3600000;
inline bool validDuration(uint32_t duration) { return duration >= 100 && duration <= MAX_PULSE_MS; }
struct Timer {
    bool running = false;
    uint32_t started = 0, duration = 0;
    void start(uint32_t now, uint32_t ms) { running = true; started = now; duration = ms; }
    void cancel() { running = false; }
    uint32_t remaining(uint32_t now) const {
        const uint32_t elapsed = now - started;
        return running && elapsed < duration ? duration - elapsed : 0;
    }
    bool expired(uint32_t now) const { return running && remaining(now) == 0; }
};
}
