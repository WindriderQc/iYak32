#include <cassert>
#include <iostream>
#include "api/IODiscovery.h"

int main() {
    assert(std::string(IODiscovery::domain("OUTPUT", "DIGITAL")) == "switch");
    assert(std::string(IODiscovery::domain("INPUT_PULLUP", "DIGITAL")) == "binary_sensor");
    assert(std::string(IODiscovery::domain("INPUT", "ANALOG_INPUT")) == "sensor");
    for (const char* component : {"switch", "binary_sensor", "sensor"}) {
        JsonDocument doc;
        std::string label(64, 'a');
        IODiscovery::build(doc, "ESP_TEST01", 15, label.c_str(), component, "http://192.0.2.15/io_control.html");
        assert(doc["unique_id"] == "ESP_TEST01_gpio15");
        assert(doc["device"]["identifiers"][0] == "ESP_TEST01");
        assert(doc["state_topic"] == "esp32/ESP_TEST01/io/15/state");
        assert(doc["availability"].size() == 2 && doc["availability_mode"] == "all");
        assert(doc["availability"][0]["topic"] == "sensors/ESP_TEST01/availability");
        assert(doc["availability"][1]["value_template"].as<std::string>().find("is number") != std::string::npos);
        if (std::string(component) == "switch") {
            assert(doc["command_topic"] == "esp32/ESP_TEST01/io/15/set");
            assert(doc["payload_on"] == "ON" && doc["payload_off"] == "OFF");
            assert(doc["optimistic"].is<bool>() && !doc["optimistic"].as<bool>());
            assert(doc["retain"].is<bool>() && !doc["retain"].as<bool>());
            assert(doc["value_template"].as<std::string>().find("value_json.active") != std::string::npos);
        } else {
            assert(doc["command_topic"].isNull());
            assert(doc["value_template"].as<std::string>().find("value_json.value") != std::string::npos);
        }
        // Bound individual discovery messages below the 2048-byte MQTT packet limit, including topic/header.
        assert(measureJson(doc) + 100 < 2048);
        IODiscovery::build(doc, "ESP_TEST01", 15, "Renommée", component, "http://192.0.2.15/io_control.html");
        assert(doc["unique_id"] == "ESP_TEST01_gpio15" && doc["name"] == "Renommée");
    }
    std::cout << "PASS: HA switch/input/ADC discovery, stable identities, shared device, per-pin availability, confirmed logical state, payload budget\n";
}
