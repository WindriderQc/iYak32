#include "fixtures/Arduino.h"
#include "fixtures/IOControlPlatform.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#define IO_CONTROL_PLATFORM_HEADER "../../tests/fixtures/IOControlPlatform.h"
#define time IOTest::clockTime
#include "../src/api/IOControl.cpp"
#undef time

static void receive(const char* suffix, const char* payload) {
    assert(IOControl::receive(String("esp32/ESP_FIXTURE/") + suffix,
        reinterpret_cast<const byte*>(payload), strlen(payload)));
}
static void at(int day, int hour) {
    std::tm date{}; date.tm_year = 126; date.tm_mon = 0; date.tm_mday = day;
    date.tm_hour = hour; date.tm_isdst = -1; IOTest::epoch = std::mktime(&date);
    IOTest::milliseconds += 1000;
}
static JsonDocument report() {
    JsonDocument state;
    for (auto it = IOTest::publications.rbegin(); it != IOTest::publications.rend(); ++it)
        if (it->topic == "esp32/ESP_FIXTURE/state") { assert(!deserializeJson(state,it->payload)); return state; }
    assert(false); return state;
}
int main() {
    setenv("TZ", "UTC0", 1); tzset(); at(1,12);
    assert(!deserializeJson(IOTest::config,R"({"io_pins":[{"gpio":15,"label":"Old","mode":"OUTPUT","type":"DIGITAL","inverted":true,"initial_state":"HIGH"},{"gpio":21,"label":"Reserved","mode":"OUTPUT","type":"DIGITAL"}]})"));
    IOTest::outputs[15] = true; IOTest::outputs[21] = false;
    IOTest::files["/io_timers.json"] = R"([{"io":15,"tStart":"06:00:00","tStop":"23:00:00","enabled":true}])";
    assert(IOControl::loadBootTimers() && IOControl::bootOff(15) && !IOControl::bootOff(21));
    IOControl::begin(); assert(!IOTest::outputs[15]); IOControl::tick();
    assert(IOTest::publications.size() == 2 && IOTest::publications.back().topic == "esp32/config");
    assert(IOTest::publications.back().payload == "ESP_FIXTURE");
    const size_t published = IOTest::publications.size();
    IOControl::tick(); assert(IOTest::publications.size() == published);
    for (const auto& message : IOTest::publications) assert(!message.retain);
    auto state = report(); assert(state["version"] == 1 && state["outputs"][0]["inverted"] == true);
    assert(state["outputs"][1]["available"] == false && state["outputs"][1]["active"] == false);
    receive("configIOs",R"([{"io":15,"mode":"OUT","lbl":"D15","isA":false,"name":"Lamp"}])");
    IOControl::tick(); state = report();
    assert(state["config"][0]["lbl"] == "D15" && state["config"][0]["name"] == "Lamp");
    const auto before = IOTest::files["/io_config.json"];
    IOTest::failSave = true;
    receive("configIOs",R"([{"io":15,"mode":"OUT","lbl":"D15","isA":false,"name":"Rejected"}])");
    IOControl::tick(); assert(IOTest::files["/io_config.json"] == before && IOTest::config["io_pins"][0]["name"] == "Lamp");
    assert(!report()["lastError"].isNull()); IOTest::failSave = false;
    const auto timers = IOTest::files["/io_timers.json"];
    receive("io/sunrise","15:23:00:00"); assert(IOTest::files["/io_timers.json"] == timers);
    IOTest::milliseconds += 20000;
    receive("io/nightfall","15:06:00:00"); assert(IOTest::files["/io_timers.json"] == timers);
    receive("io/sunrise","15:23:00:00"); assert(IOTest::files["/io_timers.json"] != timers);
    IOTest::connected = false; IOControl::tick();
    at(1,23); IOControl::tick(); assert(IOTest::outputs[15]);
    at(2,6); IOControl::tick(); assert(!IOTest::outputs[15]);
    const auto offlineCount = IOTest::publications.size();
    IOTest::connected = true; IOControl::tick();
    assert(IOTest::publications.size() == offlineCount + 2);
    at(2,23); IOControl::tick(); assert(IOTest::outputs[15]);
    IOTest::failSave = true; receive("io/timer/disable","15");
    assert(IOTest::outputs[15]); // A failed durable cancellation does not pretend to apply.
    IOTest::failSave = false; receive("io/timer/disable","15"); IOControl::tick();
    assert(!IOTest::outputs[15] && report()["timers"][0]["enabled"] == false);
    assert(!IOControl::receive("esp32/OTHER/io/sunrise",reinterpret_cast<const byte*>("15:06:00:00"),11));
    receive("io/sunrise","21:06:00:00"); IOControl::tick(); assert(!report()["lastError"].isNull());
    for (int io = 0; io < 32; ++io) receive("io/timer/disable",std::to_string(io).c_str());
    IOControl::tick(); assert(report()["timers"].size() == 32);
    const unsigned writes = IOTest::writes;
    receive("io/timer/disable","32"); IOControl::tick();
    assert(IOTest::writes == writes && report()["timers"].size() == 32 && !report()["lastError"].isNull());
    if (const char* output = std::getenv("IYAK_STATE_FIXTURE")) { std::ofstream file(output); serializeJson(report(),file); }
    std::cout << "PASS: actual firmware control handler, cold boot, state/boot wire, durable profile and timer failures, pairing/expiry, offline overnight execution, reconnection, cancellation and reserved GPIO refusal\n";
}
