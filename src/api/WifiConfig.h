#pragma once

#include <ArduinoJson.h>
#include <cctype>

namespace WifiConfig {

// An omitted or blank password in a settings update keeps the saved secret.
inline bool preservePassword(JsonDocument& incoming, const JsonDocument& saved) {
    auto isBlank = [](JsonVariantConst value) {
        const char* text = value | "";
        while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
        return *text == '\0';
    };

    if (!isBlank(incoming["pass"]) || isBlank(saved["pass"])) return false;
    return incoming["pass"].set(saved["pass"]);
}

} // namespace WifiConfig
