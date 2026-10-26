#include <cassert>
#include <iostream>
#include "api/IOConfig.h"
#include "api/IOScenes.h"
#include "api/IODiscovery.h"

int main() {
    JsonDocument config;
    assert(!deserializeJson(config, R"({"io_pins":[{"gpio":4,"label":"A","mode":"OUTPUT","pulse_presets_ms":[100,5000,60000]},{"gpio":15,"label":"B","mode":"OUTPUT","inverted":true},{"gpio":21,"label":"I2C","mode":"OUTPUT"},{"gpio":32,"label":"Soil","type":"ANALOG_INPUT","calibration":{"raw_min":3500,"raw_max":500,"value_min":0,"value_max":100,"unit":"%","clamp":true}}],"scenes":[{"id":"test","name":"Test","actions":[{"gpio":4,"active":false},{"gpio":15,"active":true,"duration_ms":100}]}]})"));
    assert(IOConfig::validate(config) == nullptr);
    JsonObject c = config["io_pins"][3]["calibration"].as<JsonObject>();
    assert(IOCalibration::convert(3500, c) == 0 && IOCalibration::convert(500, c) == 100);
    assert(IOCalibration::convert(2000, c) == 50 && IOCalibration::convert(0, c) == 100 && IOCalibration::convert(4095, c) == 0);
    c["clamp"] = false; assert(IOCalibration::convert(0, c) > 100); c["clamp"] = true;
    JsonDocument discovery;
    IODiscovery::build(discovery, "ESP_TEST01", 32, "Soil", "sensor", "http://device/io_control.html", c);
    assert(discovery["value_template"] == "{{ value_json.calibrated_value }}" && discovery["unit_of_measurement"] == "%");
    assert(discovery["unique_id"] == "ESP_TEST01_gpio32");
    std::vector<IOScenes::Action> actions; std::vector<int> skipped; std::string name;
    auto available = [](int gpio) { return gpio == 4 || gpio == 15; };
    assert(!IOScenes::plan(config, "test", available, actions, skipped, name));
    assert(actions.size() == 2 && actions[1].active && actions[1].duration == 100 && name == "Test");
    assert(!IOScenes::plan(config, "all_off", available, actions, skipped, name));
    assert(actions.size() == 2 && skipped.size() == 1 && skipped[0] == 21 && !actions[0].active && actions[0].duration == 0);
    config["scenes"][0]["actions"][1]["gpio"] = 21;
    assert(IOConfig::validate(config) == nullptr); // Saved usages may become available on another board.
    assert(IOScenes::plan(config, "test", available, actions, skipped, name) && actions.empty());
    assert(IOScenes::plan(config, "missing", available, actions, skipped, name));
    config["scenes"][0]["actions"][1]["gpio"] = 15;
    auto invalid = [&](const char* path) { (void)path; assert(IOConfig::validate(config) != nullptr); };
    c["raw_max"] = 3500; invalid("identical reference"); c["raw_max"] = 500;
    c["raw_min"] = -1; invalid("negative ADC"); c["raw_min"] = 3500;
    c["value_min"] = "0"; invalid("numeric string"); c["value_min"] = 0;
    config["io_pins"][0]["pulse_presets_ms"][0] = 0; invalid("zero duration"); config["io_pins"][0]["pulse_presets_ms"][0] = 5000; invalid("duplicate durations"); config["io_pins"][0]["pulse_presets_ms"][0] = 100;
    config["scenes"][0]["actions"][1]["active"] = false; invalid("OFF with duration"); config["scenes"][0]["actions"][1]["active"] = true;
    config["scenes"][0]["actions"][1]["gpio"] = 32; invalid("analog scene action"); config["scenes"][0]["actions"][1]["gpio"] = 4; invalid("duplicate action"); config["scenes"][0]["actions"][1]["gpio"] = 15;
    config["scenes"][0]["id"] = "all_off"; invalid("reserved id"); config["scenes"][0]["id"] = "test";
    assert(IOConfig::validate(config) == nullptr);
    std::cout << "PASS: reversed/clamped ADC calibration, raw/calibrated HA contract, pulse presets, validated scene plans, all-OFF skipping and unavailable preflight without partial execution\n";
}
