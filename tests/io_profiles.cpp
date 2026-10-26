#include "api/IOProfile.h"
#include <cassert>
#include <iostream>

int main() {
    JsonDocument board, patch;
    assert(!deserializeJson(board, R"({"device":{"name":"Fixture"},"io_pins":[{"gpio":15,"label":"Old","mode":"OUTPUT","type":"DIGITAL","inverted":true,"initial_state":"HIGH","graph":true,"zone":"Fixture"},{"gpio":32,"label":"Probe","mode":"INPUT","type":"ANALOG_INPUT"}],"automation":{"paused":true,"timezone":"UTC","schedules":[],"rules":[]}})"));
    assert(!deserializeJson(patch, R"([{"io":15,"mode":"OUT","lbl":"D15","isA":false,"name":"Lamp"}])"));
    const auto reserved = [](int gpio) { return gpio == 21 || gpio == 22; };
    assert(IOProfile::merge(board, patch, reserved));
    patch.clear(); // The saved strings must outlive the received message/document.
    assert(board["io_pins"][0]["lbl"] == "D15" && board["io_pins"][0]["name"] == "Lamp");
    assert(board["io_pins"][0]["label"] == "Lamp" && board["io_pins"][0]["zone"] == "Fixture");
    assert(board["io_pins"][0]["inverted"] == true && board["io_pins"][0]["initial_state"] == "HIGH");
    assert(board["io_pins"][1]["label"] == "Probe" && board["automation"]["paused"] == true);
    assert(board["device"]["name"] == "Fixture");
    std::string saved; serializeJson(board, saved);
    for (const char* invalid : {
        R"([{"io":21,"mode":"OUT","lbl":"Reserved","isA":false,"name":""}])",
        R"([{"io":34,"mode":"OUT","lbl":"Input only","isA":false,"name":""}])",
        R"([{"io":34,"mode":"INPULL","lbl":"Input only","isA":false,"name":""}])",
        R"([{"io":6,"mode":"IN","lbl":"Flash","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT","lbl":"Analog","isA":true,"name":""}])",
        R"([{"io":15,"mode":"OUTPUT","lbl":"Mode","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT\u0000hidden","lbl":"Mode","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT","lbl":"NUL\u0000suffix","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT","lbl":"Control\n","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT","lbl":"Extra","isA":false,"name":"","inverted":false}])",
        R"([{"io":15,"mode":"OUT","lbl":"First","isA":false,"name":""},{"io":15,"mode":"IN","lbl":"Second","isA":false,"name":""}])",
        R"([{"io":15,"mode":"OUT","lbl":"Partial","isA":false,"name":""},{"io":6,"mode":"IN","lbl":"Invalid","isA":false,"name":""}])"
    }) {
        assert(!deserializeJson(patch, invalid));
        assert(!IOProfile::merge(board, patch, reserved));
        std::string unchanged; serializeJson(board, unchanged); assert(unchanged == saved);
    }
    assert(!deserializeJson(patch, R"([{"io":4,"mode":"OUT","lbl":"","isA":false,"name":""}])"));
    assert(IOProfile::merge(board, patch, reserved));
    assert(board["io_pins"][2]["label"] == "GPIO 4");
    assert(board["io_pins"][2]["initial_state"] == "LOW"); // New output is logically OFF.
    JsonDocument transition, turnIntoOutput;
    assert(!deserializeJson(transition, R"({"io_pins":[{"gpio":15,"label":"Input","mode":"INPUT","initial_state":"LOW","inverted":true}]})"));
    assert(!deserializeJson(turnIntoOutput, R"([{"io":15,"mode":"OUT","lbl":"D15","isA":false,"name":"New output"}])"));
    assert(IOProfile::merge(transition, turnIntoOutput, reserved));
    assert(transition["io_pins"][0]["inverted"] == true && transition["io_pins"][0]["initial_state"] == "HIGH");
    std::string accented;
    for (int n = 0; n < 64; ++n) accented += "é";
    patch[0]["name"] = accented;
    assert(IOProfile::merge(board, patch, reserved));
    accented += "é"; patch[0]["name"] = accented;
    assert(!IOProfile::merge(board, patch, reserved));
    std::string astral;
    for (int n = 0; n < 32; ++n) astral += "🌱";
    patch[0]["name"] = astral;
    assert(IOProfile::merge(board, patch, reserved));
    astral += "🌱"; patch[0]["name"] = astral;
    assert(!IOProfile::merge(board, patch, reserved));
    JsonDocument report;
    IOProfile::describe(board, report.to<JsonArray>());
    assert(report[0]["lbl"] == "D15" && report[0]["name"] == "Lamp" && report[0]["mode"] == "OUT");
    assert(report[1]["mode"] == "IN" && report[1]["isA"] == true);
    JsonDocument crowded;
    JsonArray pins = crowded["io_pins"].to<JsonArray>();
    for (int gpio : {0,1,2,3,4,5,12,13,14,15,16,17,18,19,23,25,26,27,32,33,34,35,36,39}) {
        JsonObject pin = pins.add<JsonObject>(); pin["gpio"] = gpio;
        pin["mode"] = "INPUT"; pin["label"] = std::string(64,'x');
    }
    bool reachedLimit = false;
    for (int gpio : {0,1,2,3,4,5,12,13,14,15,16,17,18,19,23,25}) {
        JsonDocument smallPatch;
        JsonObject row = smallPatch.to<JsonArray>().add<JsonObject>();
        row["io"] = gpio; row["mode"] = "IN"; row["lbl"] = std::string(64,'x');
        row["name"] = std::string(64,'y'); row["isA"] = false;
        std::string previous; serializeJson(crowded,previous);
        if (!IOProfile::merge(crowded,smallPatch,reserved)) {
            std::string unchanged; serializeJson(crowded,unchanged); assert(unchanged == previous);
            reachedLimit = true; break;
        }
    }
    assert(reachedLimit); // Small patches cannot make the full report exceed Data's budget.
    std::cout << "PASS: atomic legacy patch, full name/label ownership, native settings/polarity, reserved pins, invalid/partial arrays, Unicode limits and applied report\n";
}
