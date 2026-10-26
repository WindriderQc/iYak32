#pragma once
#include "IOEvents.h"
#include <cmath>

namespace IOActivity {
enum class Kind : uint8_t { GPIO, RULE, SCHEDULE, SCENE, ALERT_ACTIVE, ALERT_CLEAR };
inline const char* name(Kind kind) {
    switch (kind) {
        case Kind::RULE: return "rule"; case Kind::SCHEDULE: return "schedule"; case Kind::SCENE: return "scene";
        case Kind::ALERT_ACTIVE: return "alert_active"; case Kind::ALERT_CLEAR: return "alert_clear";
        default: return "gpio";
    }
}
struct Event {
    IOEvents::Event io;
    Kind kind = Kind::GPIO;
    bool ok = true;
    double reading = NAN;
    char ref[33] = {}, message[129] = {};
};
template<size_t Capacity> class Journal {
    Event events_[Capacity]; size_t start_ = 0, count_ = 0; uint32_t sequence_ = 0;
public:
    void add(Event event) {
        event.io.id = ++sequence_;
        if (count_ < Capacity) events_[(start_ + count_++) % Capacity] = event;
        else { events_[start_] = event; start_ = (start_ + 1) % Capacity; }
    }
    size_t size() const { return count_; }
    uint32_t latest() const { return sequence_; }
    uint32_t oldest() const { return count_ ? events_[start_].io.id : sequence_ + 1; }
    const Event& at(size_t index) const { return events_[(start_ + index) % Capacity]; }
};
}
