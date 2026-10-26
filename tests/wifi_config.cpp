#include "api/WifiConfig.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    // Settings updates can omit secrets returned redacted by GET /data.
    for (const char* update : {
        R"({"mqtturl":"new-broker"})",
        R"({"mqtturl":"new-broker","pass":null})",
        R"({"mqtturl":"new-broker","pass":""})",
        R"({"mqtturl":"new-broker","pass":" \t\r\n"})"
    }) {
        JsonDocument incoming;
        assert(!deserializeJson(incoming, update));
        {
            JsonDocument saved;
            assert(!deserializeJson(saved, R"({"pass":"original-secret"})"));
            assert(WifiConfig::preservePassword(incoming, saved));
        }
        // The incoming document must own the password after saved is destroyed.
        assert(std::strcmp(incoming["pass"].as<const char*>(), "original-secret") == 0);
        assert(std::strcmp(incoming["mqtturl"].as<const char*>(), "new-broker") == 0);
    }

    JsonDocument incoming;
    JsonDocument saved;
    assert(!deserializeJson(incoming, R"({"pass":"replacement-secret"})"));
    assert(!deserializeJson(saved, R"({"pass":"original-secret"})"));
    assert(!WifiConfig::preservePassword(incoming, saved));
    assert(std::strcmp(incoming["pass"].as<const char*>(), "replacement-secret") == 0);

    for (const char* config : {"{}", R"({"pass":null})", R"({"pass":""})", R"({"pass":"  "})"}) {
        incoming.clear();
        assert(!deserializeJson(saved, config));
        assert(!WifiConfig::preservePassword(incoming, saved));
        assert(incoming["pass"].isNull());
    }

    incoming.clear();
    assert(!deserializeJson(saved, R"({"pass":"  original-secret  "})"));
    assert(WifiConfig::preservePassword(incoming, saved));
    assert(std::strcmp(incoming["pass"].as<const char*>(), "  original-secret  ") == 0);

    std::cout << "PASS: omitted, null and blank passwords preserve the saved secret; "
                 "explicit replacements and missing saved credentials are handled correctly.\n";
}
