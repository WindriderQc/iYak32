#include "IOControl.h"
#include "DailyTimers.h"
#include "IOProfile.h"
#ifndef IO_CONTROL_PLATFORM_HEADER
#define IO_CONTROL_PLATFORM_HEADER "IOControlPlatform.h"
#endif
#include IO_CONTROL_PLATFORM_HEADER

namespace IOControl {
namespace {
constexpr const char* TIMER_FILE = "/io_timers.json";
DailyTimers::Schedule schedules[DailyTimers::PINS];
DailyTimers::Runtime runtimes[DailyTimers::PINS];
DailyTimers::Pending pairs[DailyTimers::PINS];
bool exists[DailyTimers::PINS] = {};
bool dirty = true, wasConnected = false, timersLoaded = false;
uint32_t reportedAt = 0, serial = 0;
char bootId[17] = {};
String lastError;

bool writeOutput(int io, bool active) {
    const bool written = Esp32::setPinActive(io, active, 0, IOEvents::Source::TIMER);
    if (written) dirty = true;
    return written;
}
bool saveTimer(unsigned io, const DailyTimers::Schedule& schedule) {
    JsonDocument doc;
    JsonArray rows = doc.to<JsonArray>();
    for (unsigned pin = 0; pin < DailyTimers::PINS; ++pin) {
        if (!exists[pin] && pin != io) continue;
        const auto& item = pin == io ? schedule : schedules[pin];
        JsonObject row = rows.add<JsonObject>(); char on[9], off[9];
        DailyTimers::formatTime(item.on,on); DailyTimers::formatTime(item.off,off);
        row["io"] = pin; row["tStart"] = on; row["tStop"] = off; row["enabled"] = item.enabled;
    }
    if (rows.size() > 32) return false;
    String json; serializeJson(doc,json);
    if (!Esp32::spiffsMounted || !Storage::writeConfigFile(TIMER_FILE,json)) return false;
    schedules[io] = schedule; exists[io] = true; runtimes[io].reset();
    writeOutput(io,false); lastError = ""; dirty = true; return true;
}
bool applyProfile(const byte* message, unsigned length) {
    JsonDocument incoming;
    if (length > 3800 || deserializeJson(incoming,message,length)) return false;
    // Retain the board's existing native-document MQTT interface as well.
    if (!incoming.is<JsonArray>() && !incoming.is<JsonObject>()) return false;
    const bool applied = Esp32::mergeLegacyIOConfiguration(incoming);
    if (applied) { lastError = ""; dirty = true; }
    return applied;
}
void report(time_t now, bool clockValid) {
    JsonDocument doc, native, status;
    Esp32::getIOControlSnapshot(native,status);
    doc["version"] = 1; doc["bootId"] = bootId; doc["serial"] = ++serial;
    doc["clockValid"] = clockValid;
    std::tm local{}; localtime_r(&now,&local); char localText[32] = {}, zone[32] = {};
    if (clockValid) { strftime(localText,sizeof(localText),"%Y-%m-%d %H:%M:%S",&local); strftime(zone,sizeof(zone),"UTC%z",&local); }
    doc["timezone"] = zone; doc["localTime"] = localText;
    if (lastError.length()) doc["lastError"] = lastError; else doc["lastError"] = nullptr;
    IOProfile::describe(native,doc["config"].to<JsonArray>());
    JsonArray outputs = doc["outputs"].to<JsonArray>(), timers = doc["timers"].to<JsonArray>();
    for (JsonVariantConst pin : status["statuses"].as<JsonArrayConst>()) {
        if (strcmp(pin["mode"] | "INPUT", "OUTPUT") || !strcmp(pin["type"] | "DIGITAL", "ANALOG_INPUT")) continue;
        JsonObject row = outputs.add<JsonObject>(); row["io"] = pin["gpio"];
        row["available"] = pin["active"].is<bool>(); row["active"] = pin["active"].as<bool>();
        row["inverted"] = pin["inverted"] | false;
    }
    for (unsigned io = 0; io < DailyTimers::PINS; ++io) if (exists[io]) {
        JsonObject row = timers.add<JsonObject>(); char on[9],off[9]; DailyTimers::formatTime(schedules[io].on,on); DailyTimers::formatTime(schedules[io].off,off);
        row["io"] = io; row["tStart"] = on; row["tStop"] = off; row["enabled"] = schedules[io].enabled;
        if (clockValid && Esp32::isPinOutputAvailable(io) && schedules[io].enabled) {
            time_t next = DailyTimers::nextStart(now,schedules[io].on); std::tm utc{}; gmtime_r(&next,&utc); char text[24];
            strftime(text,sizeof(text),"%Y-%m-%dT%H:%M:%SZ",&utc); row["nextStart"] = text;
        } else row["nextStart"] = nullptr;
    }
    String payload; serializeJson(doc,payload);
    if (payload.length() <= 8192 && Mqtt::mqttClient.publish((String("esp32/")+Esp32::DEVICE_NAME+"/state").c_str(),payload.c_str(),false)) {
        dirty = false; reportedAt = millis();
    }
}
} // namespace
bool loadBootTimers() {
    if (timersLoaded) return true;
    if (!Esp32::spiffsMounted) return false;
    const String text = Storage::readConfigFile(TIMER_FILE);
    if (text.isEmpty()) { timersLoaded = true; return true; }
    JsonDocument doc;
    if (deserializeJson(doc,text) || !doc.is<JsonArrayConst>() || doc.size() > 32) { lastError = "Invalid saved timers; I/O boot configuration refused"; return false; }
    bool seen[DailyTimers::PINS] = {};
    for (JsonVariantConst row : doc.as<JsonArrayConst>()) {
        unsigned on,off;
        if (!row.is<JsonObjectConst>() || row.size() != 4 || !row["io"].is<unsigned>() || row["io"].as<unsigned>() >= DailyTimers::PINS || !row["enabled"].is<bool>() ||
            !DailyTimers::parseTime(row["tStart"],on) || !DailyTimers::parseTime(row["tStop"],off) || on == off) { lastError = "Invalid saved timer; I/O boot configuration refused"; return false; }
        unsigned io = row["io"];
        if (seen[io]) { lastError = "Duplicate saved timer GPIO; I/O boot configuration refused"; return false; }
        seen[io] = true;
    }
    for (JsonVariantConst row : doc.as<JsonArrayConst>()) {
        unsigned on,off; DailyTimers::parseTime(row["tStart"],on); DailyTimers::parseTime(row["tStop"],off);
        unsigned io = row["io"]; schedules[io] = {on,off,row["enabled"].as<bool>()}; exists[io] = true;
    }
    timersLoaded = true; return true;
}
bool bootOff(unsigned io) { return io < DailyTimers::PINS && exists[io] && schedules[io].enabled; }
void begin() {
    snprintf(bootId,sizeof(bootId),"%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
    if (!Mqtt::mqttClient.setBufferSize(9216)) lastError = "MQTT control buffer allocation failed";
    loadBootTimers();
    for (unsigned io = 0; io < DailyTimers::PINS; ++io) if (bootOff(io)) writeOutput(io,false);
}
void changed() { dirty = true; }
bool receive(const String& topic, const byte* message, unsigned length) {
    const String base = String("esp32/")+Esp32::DEVICE_NAME;
    if (topic == base+"/configIOs") {
        if (!applyProfile(message,length)) { lastError = "Profile refused or could not be saved; previous configuration retained"; dirty = true; }
        return true;
    }
    const bool sunrise = topic == base+"/io/sunrise", nightfall = topic == base+"/io/nightfall", disable = topic == base+"/io/timer/disable";
    if (!sunrise && !nightfall && !disable) return false;
    if (!length || length > 12) { lastError = "Invalid timer message"; dirty = true; return true; }
    char text[13] = {}; memcpy(text,message,length);
    unsigned io = 0,seconds = 0;
    if (strlen(text) != length) { lastError = "Invalid timer message"; dirty = true; return true; }
    if (disable) {
        bool valid = length <= 2;
        for (unsigned n = 0; n < length; ++n) { if (text[n] < '0' || text[n] > '9') valid = false; io = io*10+text[n]-'0'; }
        if (!valid || io >= DailyTimers::PINS) { lastError = "Invalid timer GPIO"; dirty = true; return true; }
        pairs[io].cancel(); auto disabled = schedules[io]; disabled.enabled = false;
        if (!saveTimer(io,disabled)) lastError = "Could not save timer cancellation";
    } else {
        if (!DailyTimers::parseMessage(text,io,seconds) || !Esp32::isPinOutputAvailable(io)) { lastError = "Timer requires a configured available digital output"; dirty = true; return true; }
        DailyTimers::Schedule complete;
        if (pairs[io].receive(sunrise,seconds,millis(),complete) && !saveTimer(io,complete)) lastError = "Could not save complete timer";
    }
    dirty = true; return true;
}
void tick() {
    const time_t now = time(nullptr); const bool valid = now > 1510644967;
    for (unsigned io = 0; io < DailyTimers::PINS; ++io) if (exists[io]) runtimes[io].tick(schedules[io],now,valid,Esp32::isPinOutputAvailable(io),[io](bool active){ return writeOutput(io,active); });
    const bool connected = Mqtt::isEnabled && Mqtt::mqttClient.connected();
    if (connected && (!wasConnected || dirty || millis()-reportedAt >= 30000)) report(now,valid);
    if (connected && !wasConnected && Esp32::isConfigFromServer) Mqtt::mqttClient.publish("esp32/config",Esp32::DEVICE_NAME.c_str(),false);
    wasConnected = connected;
}
} // namespace IOControl
