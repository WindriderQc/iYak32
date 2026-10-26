#include "api/IOConfig.h"
#include <cassert>
#include <iostream>

int main() {
    auto valid = [](const char* json) {
        JsonDocument doc;
        assert(!deserializeJson(doc, json));
        return IOConfig::validate(doc) == nullptr;
    };
    assert(valid(R"({"io_pins":[]})"));
    assert(valid(R"({"io_pins":[{"gpio":15,"label":"Relay","mode":"OUTPUT","inverted":true}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"Relay","inverted":"true"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":17}]})"));
    assert(valid(R"({"io_pins":[{"gpio":15,"label":"LED","mode":"OUTPUT","initial_state":"LOW","type":"DIGITAL"}]})"));
    assert(valid(R"({"io_pins":[{"gpio":39,"label":"ADC","mode":"INPUT","type":"ANALOG_INPUT"}]})"));
    assert(valid(R"({"io_pins":[{"gpio":34,"label":"Legacy input","mode":"INPUT_PULLUP"}]})"));
    assert(!valid(R"({})"));
    assert(!valid(R"({"io_pins":null})"));
    assert(!valid(R"({"io_pins":[null]})"));
    assert(!valid(R"({"io_pins":[{"gpio":"15","label":"LED"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15.5,"label":"LED"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":6,"label":"Flash"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":40,"label":"Invalid"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"   "}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"LED"},{"gpio":15,"label":"Duplicate"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":34,"label":"Input only","mode":"OUTPUT"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"LED","mode":"PWM"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"LED","type":"ANALOG_OUTPUT"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"LED","mode":"OUTPUT","type":"ANALOG_INPUT"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":18,"label":"No ADC","type":"ANALOG_INPUT"}]})"));
    assert(!valid(R"({"io_pins":[{"gpio":15,"label":"LED","initial_state":"ON"}]})"));
    assert(IOConfig::available(0) && IOConfig::available(39));
    assert(!IOConfig::available(-1) && !IOConfig::available(6) && !IOConfig::available(37));
    assert(IOConfig::analog(39) && !IOConfig::analog(18));
    assert(IOConfig::adc2(15) && !IOConfig::adc2(39));
    std::cout << "PASS: I/O configuration validation, legacy input-only pulls, ADC capabilities and duplicate GPIO rejection\n";
}
