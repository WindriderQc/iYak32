#pragma once
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <cstring>

namespace IOScenes {
struct Action { int gpio; bool active; uint32_t duration; };
// Build the entire plan before the caller changes any pin. Availability can change with board usages.
template<typename Available>
const char* plan(const JsonDocument& config, const char* id, Available available,
                 std::vector<Action>& actions, std::vector<int>& skipped, std::string& name) {
    actions.clear(); skipped.clear();
    if (!std::strcmp(id, "all_off")) {
        name = "Tout OFF";
        for (JsonVariantConst pin : config["io_pins"].as<JsonArrayConst>()) {
            if (std::strcmp(pin["mode"] | "INPUT", "OUTPUT") || std::strcmp(pin["type"] | "DIGITAL", "DIGITAL")) continue;
            int gpio = pin["gpio"];
            if (available(gpio)) actions.push_back({gpio, false, 0}); else skipped.push_back(gpio);
        }
        return actions.empty() ? "Aucune sortie disponible." : nullptr;
    }
    for (JsonVariantConst scene : config["scenes"].as<JsonArrayConst>()) {
        if (std::strcmp(scene["id"] | "", id)) continue;
        name = scene["name"].as<std::string>();
        for (JsonVariantConst action : scene["actions"].as<JsonArrayConst>()) {
            int gpio = action["gpio"];
            if (!available(gpio)) { actions.clear(); return "Une sortie de cette scène est indisponible. Aucune commande exécutée."; }
            actions.push_back({gpio, action["active"], action["duration_ms"] | 0U});
        }
        return actions.empty() ? "Cette scène n'a aucune action." : nullptr;
    }
    return "Scène inconnue.";
}
}
