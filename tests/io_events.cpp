#include <cassert>
#include <iostream>
#include <string>
#define INPUT 0x01
#include "api/IOEvents.h"

int main() {
    IOEvents::Journal<64> journal;
    assert(journal.size() == 0 && journal.latest() == 0 && journal.oldest() == 1);
    IOEvents::Event event;
    event.gpio = 15; event.active = true; event.value = 0;
    event.source = IOEvents::Source::MQTT; event.kind = IOEvents::Kind::PULSE;
    event.duration = 100; event.at = 0xfffffffe;
    std::string label(100, 'a'), scene(100, 's'); journal.add(event, label.c_str(), scene.c_str()); label[0] = 'b'; scene[0] = 'x';
    assert(journal.at(0).label[0] == 'a' && std::strlen(journal.at(0).label) == 64);
    assert(journal.at(0).scene[0] == 's' && std::strlen(journal.at(0).scene) == 64);
    for (int i = 0; i < 69; ++i) { event.at = i; journal.add(event, "Led3"); }
    assert(journal.size() == 64 && journal.latest() == 70 && journal.oldest() == 7);
    for (size_t i = 0; i < journal.size(); ++i) {
        const auto& stored = journal.at(i);
        assert(stored.id == i + 7 && stored.gpio == 15 && stored.active && stored.value == 0);
        assert(stored.duration == 100 && stored.at == i + 5);
        assert(std::string(stored.label) == "Led3");
    }
    assert(std::string(IOEvents::name(IOEvents::Source::INPUT_SIGNAL)) == "input");
    assert(std::string(IOEvents::name(IOEvents::Source::BUTTON)) == "button");
    assert(std::string(IOEvents::name(IOEvents::Source::TIMER)) == "timer");
    assert(std::string(IOEvents::name(IOEvents::Kind::EXPIRED)) == "expired");
    std::cout << "PASS: bounded 64-event journal, eviction/order, copied/truncated labels, logical and raw states, origins\n";
}
