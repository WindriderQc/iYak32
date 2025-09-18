#include "ConfigManager.h"
#include "Storage.h"
#include "JsonTools.h"
#include <SPIFFS.h>

ConfigManager::ConfigManager() : isLoaded_(false) {
    // Constructor
}

bool ConfigManager::loadConfig(const String& filename) {
    if (!SPIFFS.exists(filename)) {
        Serial.print("ConfigManager: File not found: ");
        Serial.println(filename);
        isLoaded_ = false;
        return false;
    }

    String file_content = Storage::readFile(filename);
    if (file_content.isEmpty()) {
        Serial.print("ConfigManager: File is empty: ");
        Serial.println(filename);
        isLoaded_ = false;
        return false;
    }

    DeserializationError error = deserializeJson(configDoc_, file_content);
    if (error) {
        Serial.print(F("ConfigManager: deserializeJson() failed for "));
        Serial.print(filename);
        Serial.print(F(": "));
        Serial.println(error.c_str());
        isLoaded_ = false;
        return false;
    }

    Serial.print("ConfigManager: Successfully loaded and parsed ");
    Serial.println(filename);
    isLoaded_ = true;
    return true;
}

bool ConfigManager::saveConfig(const String& filename, const JsonDocument& doc) {
    String content = JsonTools::getJsonString(doc, true); // Save pretty
    if (Storage::writeFile(filename, content)) {
        // Also update the internal state to match what was just saved
        configDoc_ = doc;
        isLoaded_ = true;
        return true;
    }
    return false;
}

const JsonDocument& ConfigManager::getConfig() const {
    return configDoc_;
}

String ConfigManager::getConfigString(bool pretty) const {
    if (!isLoaded_) {
        return "{}"; // Return empty JSON object if nothing is loaded
    }
    return JsonTools::getJsonString(configDoc_, pretty);
}

bool ConfigManager::isLoaded() const {
    return isLoaded_;
}
