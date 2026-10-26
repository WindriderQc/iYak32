#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace IOEvents {
enum class Source : uint8_t { WEB, MQTT, BUTTON, TIMER, CONFIG, BOOT, INPUT_SIGNAL, SCHEDULE, RULE };
enum class Kind : uint8_t { COMMAND, PULSE, EXPIRED, CONFIGURED, REMOVED, STARTUP, INPUT_CHANGED, OVERRIDE, RESUMED };
inline const char* name(Source source) {
    switch (source) {
        case Source::WEB: return "web"; case Source::MQTT: return "mqtt";
        case Source::BUTTON: return "button"; case Source::TIMER: return "timer";
        case Source::CONFIG: return "config"; case Source::BOOT: return "boot";
        case Source::SCHEDULE: return "schedule"; case Source::RULE: return "rule";
        default: return "input";
    }
}
inline const char* name(Kind kind) {
    switch (kind) {
        case Kind::COMMAND: return "command"; case Kind::PULSE: return "pulse";
        case Kind::EXPIRED: return "expired"; case Kind::CONFIGURED: return "configured";
        case Kind::REMOVED: return "removed"; case Kind::STARTUP: return "startup";
        case Kind::OVERRIDE: return "override"; case Kind::RESUMED: return "resumed";
        default: return "input_changed";
    }
}
struct Event {
    uint32_t id = 0, at = 0, duration = 0, trace = 0;
    int gpio = -1, value = -1, previous = -1;
    bool active = false, output = true;
    Source source = Source::WEB;
    Kind kind = Kind::COMMAND;
    char label[65] = {};
    char scene[65] = {};
};
template<size_t Capacity> class Journal {
    Event events_[Capacity];
    size_t start_ = 0, count_ = 0;
    uint32_t sequence_ = 0;
public:
    static_assert(Capacity > 0, "A journal needs at least one slot");
    void add(Event event, const char* label, const char* scene = nullptr) {
        event.id = ++sequence_;
        std::strncpy(event.label, label ? label : "", sizeof(event.label) - 1);
        event.label[sizeof(event.label) - 1] = '\0';
        std::strncpy(event.scene, scene ? scene : "", sizeof(event.scene) - 1);
        event.scene[sizeof(event.scene) - 1] = '\0';
        if (count_ < Capacity) events_[(start_ + count_++) % Capacity] = event;
        else { events_[start_] = event; start_ = (start_ + 1) % Capacity; }
    }
    size_t size() const { return count_; }
    uint32_t latest() const { return sequence_; }
    uint32_t oldest() const { return count_ ? events_[start_].id : sequence_ + 1; }
    const Event& at(size_t index) const { return events_[(start_ + index) % Capacity]; }
};
}
