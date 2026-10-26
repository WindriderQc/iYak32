#include "api/IOConfig.h"
#include <cassert>
#include <iostream>
int main() {
    JsonDocument doc;deserializeJson(doc,R"({"io_pins":[],"home_assistant":{"url":"http://192.0.2.10:30103","device_id":"0123456789abcdef0123456789abcdef","entities":{"temperature":"sensor.esp_test01_temperature","gpio15":"switch.esp_test01_led3"}}})");assert(!IOConfig::validate(doc));
    for(const char* value:{"javascript:alert(1)","http://user:pass@host","http://host/path","http://host/?token=secret","https://host/#foo","http://","http:// bad"}) {doc["home_assistant"]["url"]=value;assert(IOConfig::validate(doc));}
    doc["home_assistant"]["url"]="https://ha.example/";assert(!IOConfig::validate(doc));doc["home_assistant"]["entities"]["gpio15"]="switch.BAD";assert(IOConfig::validate(doc));doc["home_assistant"]["entities"]["gpio15"]="switch.good";
    doc["home_assistant"]["token"]="secret";assert(IOConfig::validate(doc));doc["home_assistant"].remove("token");doc["home_assistant"]["device_id"]="bad";assert(IOConfig::validate(doc));doc["home_assistant"].remove("device_id");assert(!IOConfig::validate(doc));doc.remove("home_assistant");assert(!IOConfig::validate(doc));
    std::cout<<"PASS: optional public HA links, valid origins/entity/device IDs; credential/query/path/script URLs and tokens rejected\n";
}
