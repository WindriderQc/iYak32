#pragma once

#include <ArduinoJson.h>
#include <String.h>

class ConfigManager {
public:
    ConfigManager();

    // Loads a configuration file into the manager.
    bool loadConfig(const String& filename);

    // Saves a JsonDocument to the specified file.
    bool saveConfig(const String& filename, const JsonDocument& doc);

    // Returns a const reference to the loaded configuration document.
    const JsonDocument& getConfig() const;

    // Returns the loaded configuration as a serialized string.
    String getConfigString(bool pretty = false) const;

    // Checks if a configuration has been successfully loaded.
    bool isLoaded() const;

private:
    JsonDocument configDoc_;
    bool isLoaded_;
};
