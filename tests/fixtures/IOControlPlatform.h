#pragma once
#include "api/IOProfile.h"
#include <map>
#include <vector>
#include <ctime>

namespace IOTest {
inline JsonDocument config;
inline std::map<int,bool> outputs;
inline std::map<int,IOOutput::Timer> overrides;
inline unsigned startAttempts = 0;
inline std::map<std::string,std::string> files;
inline bool failSave = false, connected = true;
inline unsigned writes = 0;
inline std::vector<std::pair<int,bool>> commands;
struct Publication { std::string topic, payload; bool retain; };
inline std::vector<Publication> publications;
inline time_t epoch = 1767225600;
inline time_t clockTime(time_t*) { return epoch; }
inline bool reserved(int gpio) { return gpio == 21 || gpio == 22; }
}
namespace IOEvents { enum class Source { TIMER }; }
namespace Storage {
inline String readConfigFile(const String& path) { return IOTest::files[path]; }
inline bool writeConfigFile(const String& path, const String& content) {
    if (IOTest::failSave) return false;
    IOTest::files[path] = content; ++IOTest::writes; return true;
}
}
namespace Esp32 {
inline String DEVICE_NAME = "ESP_FIXTURE";
inline bool spiffsMounted = true, isConfigFromServer = true;
inline bool isPinOutputAvailable(int gpio) { return IOTest::outputs.count(gpio) && !IOTest::reserved(gpio); }
inline bool setPinActive(int gpio, bool active, uint32_t, IOEvents::Source) {
    if (!isPinOutputAvailable(gpio)) return false;
    IOTest::outputs[gpio] = active; IOTest::commands.emplace_back(gpio, active); return true;
}
inline const char* applyDailyTimerOutput(int gpio, bool active) {
    if (!isPinOutputAvailable(gpio)) return "Minuterie quotidienne : sortie indisponible.";
    if (active) {
        ++IOTest::startAttempts;
        if (const char* reason = IOOperator::dailyStartBlock(IOTest::config["automation"],IOTest::overrides[gpio].remaining(millis()))) return reason;
    }
    return setPinActive(gpio,active,0,IOEvents::Source::TIMER) ? nullptr : "Minuterie quotidienne : sortie refusée.";
}
inline bool mergeLegacyIOConfiguration(const JsonDocument& input) {
    JsonDocument next = IOTest::config;
    if (!IOProfile::merge(next,input,IOTest::reserved)) return false;
    std::string json; serializeJson(next,json);
    if (!Storage::writeConfigFile("/io_config.json",json)) return false;
    IOTest::config = next; return true;
}
inline void getIOControlSnapshot(JsonDocument& config, JsonDocument& status) {
    config = IOTest::config;
    JsonArray rows = status["statuses"].to<JsonArray>();
    for (JsonVariantConst pin : config["io_pins"].as<JsonArrayConst>()) {
        JsonObject row = rows.add<JsonObject>();
        row["gpio"] = pin["gpio"]; row["mode"] = pin["mode"]; row["type"] = pin["type"];
        row["inverted"] = pin["inverted"] | false;
        if (isPinOutputAvailable(pin["gpio"])) row["active"] = IOTest::outputs[pin["gpio"]];
        else row["active"] = nullptr;
    }
}
}
namespace Mqtt {
inline bool isEnabled = true;
struct Client {
    bool setBufferSize(unsigned bytes) { return bytes == 9216; }
    bool connected() { return IOTest::connected; }
    bool publish(const char* topic, const char* payload, bool retain) {
        if (!connected()) return false;
        IOTest::publications.push_back({topic,payload,retain}); return true;
    }
};
inline Client mqttClient;
}
