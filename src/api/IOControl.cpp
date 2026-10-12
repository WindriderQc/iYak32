#include "IOControl.h"
#include "DailyTimers.h"
#include "AtomicJsonStore.h"
#include "Esp32.h"
#include "Mqtt.h"
#include <SPIFFS.h>
#include <esp_system.h>

namespace IOControl {
namespace {
constexpr const char* TIMER_FILE = "/io_timers.json";
DailyTimers::Schedule schedules[DailyTimers::PINS];
DailyTimers::Runtime runtimes[DailyTimers::PINS];
DailyTimers::Pending pairs[DailyTimers::PINS];
bool exists[DailyTimers::PINS] = {};
bool dirty = true, wasConnected = false;
uint32_t reportedAt = 0, serial = 0;
char bootId[17] = {};
String lastError;

bool available(int io) {
    switch (io) {
        case 0: case 1: case 2: case 3: case 4: case 5:
        case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19:
        case 21: case 22: case 23: case 25: case 26: case 27:
        case 32: case 33: case 34: case 35: case 36: case 39: return true;
        default: return false;
    }
}
bool inputOnly(int io) { return io == 34 || io == 35 || io == 36 || io == 39; }
bool adc(int io) { return inputOnly(io) || io == 32 || io == 33 || io == 0 || io == 2 || io == 4 ||
    io == 12 || io == 13 || io == 14 || io == 15 || io == 25 || io == 26 || io == 27; }
bool reserved(int io) { return Esp32::buzzer_enabled_ && io == Esp32::configured_buzzer_pin_; }
Esp32::IO_Pin_Detail* pinOf(int io) {
    for (auto& pin : Esp32::configured_pins) if (pin.gpio == io) return &pin;
    return nullptr;
}
bool output(int io) {
    const auto* pin = pinOf(io);
    return available(io) && !reserved(io) && pin && pin->actual_mode == OUTPUT && pin->type_str != "ANALOG_INPUT";
}
bool readDoc(const String& path, JsonDocument& doc) {
    return Esp32::spiffsMounted && AtomicJsonStore::read(SPIFFS,path.c_str(),doc);
}
bool saveDoc(const String& path, const JsonDocument& doc) {
    return Esp32::spiffsMounted && AtomicJsonStore::save(SPIFFS,path.c_str(),doc);
}
bool inverted(int io) {
    JsonDocument doc; if (!readDoc(Esp32::CONFIG_IO_FILENAME, doc)) return false;
    for (JsonObjectConst row : doc["io_pins"].as<JsonArrayConst>()) if (row["gpio"].as<int>() == io) return row["inverted"] | false;
    return false;
}
bool writeOutput(int io, bool active) {
    if (!output(io)) return false;
    digitalWrite(io, active != inverted(io) ? HIGH : LOW); dirty = true; return true;
}
bool saveTimer(unsigned io, const DailyTimers::Schedule& schedule) {
    JsonDocument doc; JsonArray rows = doc.to<JsonArray>();
    for (unsigned pin = 0; pin < DailyTimers::PINS; ++pin) {
        if (!exists[pin] && pin != io) continue;
        const auto& item = pin == io ? schedule : schedules[pin];
        JsonObject row = rows.add<JsonObject>(); char on[9], off[9];
        DailyTimers::formatTime(item.on,on); DailyTimers::formatTime(item.off,off);
        row["io"] = pin; row["tStart"] = on; row["tStop"] = off; row["enabled"] = item.enabled;
    }
    if (!saveDoc(TIMER_FILE, doc)) return false;
    schedules[io] = schedule; exists[io] = true; runtimes[io].reset();
    writeOutput(io,false); lastError = ""; dirty = true; return true;
}
const char* legacyMode(const String& mode) {
    return mode == "OUTPUT" ? "OUT" : mode == "INPUT_PULLUP" ? "INPULL" : mode == "INPUT_PULLDOWN" ? "INPULLD" : "IN";
}
bool applyProfile(const byte* message, unsigned length) {
    JsonDocument incoming;
    if (length > 3800 || deserializeJson(incoming,message,length) || !incoming.is<JsonArray>() || incoming.size() > 32) return false;
    JsonDocument doc;
    if (!readDoc(Esp32::CONFIG_IO_FILENAME,doc)) doc["io_pins"].to<JsonArray>();
    JsonArray rows = doc["io_pins"].as<JsonArray>();
    if (rows.isNull()) return false;
    bool seen[DailyTimers::PINS] = {};
    for (JsonVariantConst value : incoming.as<JsonArrayConst>()) {
        if (!value.is<JsonObjectConst>() || value.size() != 5 || !value["io"].is<int>() || !value["mode"].is<const char*>() ||
            !value["isA"].is<bool>() || !value["lbl"].is<const char*>() || !value["name"].is<const char*>()) return false;
        int io = value["io"]; const char* mode = value["mode"]; const bool analog = value["isA"];
        if (!available(io) || reserved(io) || seen[io] || strlen(value["lbl"]) > 256 || strlen(value["name"]) > 256) return false;
        seen[io] = true;
        if (strcmp(mode,"OUT") && strcmp(mode,"IN") && strcmp(mode,"INPULL") && strcmp(mode,"INPULLD")) return false;
        if ((inputOnly(io) && strcmp(mode,"IN")) || (analog && (strcmp(mode,"IN") || !adc(io)))) return false;
        JsonObject row;
        for (JsonObject candidate : rows) if (candidate["gpio"].as<int>() == io) { row = candidate; break; }
        if (row.isNull()) row = rows.add<JsonObject>();
        row["gpio"] = io; row["lbl"] = value["lbl"]; row["name"] = value["name"];
        const String name = value["name"].as<String>(), label = value["lbl"].as<String>();
        row["label"] = name.length() ? name : label.length() ? label : String("GPIO ")+io;
        row["mode"] = !strcmp(mode,"OUT") ? "OUTPUT" : !strcmp(mode,"INPULL") ? "INPUT_PULLUP" : !strcmp(mode,"INPULLD") ? "INPUT_PULLDOWN" : "INPUT";
        row["type"] = analog ? "ANALOG_INPUT" : "DIGITAL";
        if (!row["initial_state"].is<const char*>()) row["initial_state"] = "LOW";
    }
    if (rows.size() > 32 || !saveDoc(Esp32::CONFIG_IO_FILENAME,doc)) return false;
    // Legacy arrays are patches. Configure just their pins, without momentarily
    // changing unrelated outputs or overwriting their local settings.
    for (JsonObjectConst value : incoming.as<JsonArrayConst>()) {
        int io = value["io"]; const char* mode = value["mode"];
        const bool analog = value["isA"];
        const String name = value["name"].as<String>(), label = value["lbl"].as<String>();
        const String friendly = name.length() ? name : label.length() ? label : String("GPIO ")+io;
        byte actual = !strcmp(mode,"OUT") ? OUTPUT : !strcmp(mode,"INPULL") ? INPUT_PULLUP : !strcmp(mode,"INPULLD") ? INPUT_PULLDOWN : INPUT;
        const bool hadOutput = output(io), high = hadOutput && digitalRead(io) == HIGH;
        if (hadOutput && actual != OUTPUT) writeOutput(io,false);
        if (actual == OUTPUT && !hadOutput) digitalWrite(io,inverted(io) ? HIGH : LOW);
        Esp32::configPin(io,mode,friendly.c_str(),analog);
        if (actual == OUTPUT && hadOutput) digitalWrite(io,high ? HIGH : LOW);
        Esp32::IO_Pin_Detail detail(io,friendly,actual == OUTPUT ? "OUTPUT" : actual == INPUT_PULLUP ? "INPUT_PULLUP" : actual == INPUT_PULLDOWN ? "INPUT_PULLDOWN" : "INPUT",
            actual,analog ? "ANALOG_INPUT" : "DIGITAL","LOW",false);
        auto* previous = pinOf(io);
        if (previous) *previous = detail; else Esp32::configured_pins.push_back(detail);
        if (actual != OUTPUT || analog) runtimes[io].reset();
    }
    lastError = ""; dirty = true; return true;
}
void report(time_t now, bool clockValid) {
    JsonDocument doc, stored; readDoc(Esp32::CONFIG_IO_FILENAME,stored);
    doc["version"] = 1; doc["bootId"] = bootId; doc["serial"] = ++serial;
    doc["clockValid"] = clockValid;
    std::tm local{}; localtime_r(&now,&local); char localText[32] = {}, zone[32] = {};
    if (clockValid) { strftime(localText,sizeof(localText),"%Y-%m-%d %H:%M:%S",&local); strftime(zone,sizeof(zone),"UTC%z",&local); }
    doc["timezone"] = zone; doc["localTime"] = localText;
    if (lastError.length()) doc["lastError"] = lastError; else doc["lastError"] = nullptr;
    JsonArray pins = doc["config"].to<JsonArray>(), outputs = doc["outputs"].to<JsonArray>(), timers = doc["timers"].to<JsonArray>();
    for (const auto& pin : Esp32::configured_pins) {
        JsonObject row = pins.add<JsonObject>(); row["io"] = pin.gpio; row["mode"] = legacyMode(pin.mode_str); row["isA"] = pin.type_str == "ANALOG_INPUT";
        row["lbl"] = pin.label; row["name"] = "";
        for (JsonObjectConst saved : stored["io_pins"].as<JsonArrayConst>()) if (saved["gpio"].as<int>() == pin.gpio) {
            row["lbl"] = saved["lbl"] | pin.label; row["name"] = saved["name"] | ""; break;
        }
        if (pin.actual_mode == OUTPUT && pin.type_str != "ANALOG_INPUT") {
            JsonObject state = outputs.add<JsonObject>(); state["io"] = pin.gpio; state["available"] = output(pin.gpio);
            const bool inv = inverted(pin.gpio); state["inverted"] = inv; state["active"] = (digitalRead(pin.gpio) == HIGH) != inv;
        }
    }
    for (unsigned io = 0; io < DailyTimers::PINS; ++io) if (exists[io]) {
        JsonObject row = timers.add<JsonObject>(); char on[9],off[9]; DailyTimers::formatTime(schedules[io].on,on); DailyTimers::formatTime(schedules[io].off,off);
        row["io"] = io; row["tStart"] = on; row["tStop"] = off; row["enabled"] = schedules[io].enabled;
        if (clockValid && output(io) && schedules[io].enabled) {
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
bool loadBootIOConfiguration(JsonDocument& doc) {
    if (!readDoc(Esp32::CONFIG_IO_FILENAME,doc)) return false;
    JsonDocument timers;
    if (readDoc(TIMER_FILE,timers) && timers.is<JsonArrayConst>()) {
        for (JsonObjectConst timer : timers.as<JsonArrayConst>()) {
            if (!timer["enabled"].is<bool>() || !timer["enabled"].as<bool>() || !timer["io"].is<unsigned>()) continue;
            for (JsonObject pin : doc["io_pins"].as<JsonArray>()) if (pin["gpio"].as<unsigned>() == timer["io"].as<unsigned>())
                pin["initial_state"] = (pin["inverted"] | false) ? "HIGH" : "LOW";
        }
    }
    return true;
}
void begin() {
    snprintf(bootId,sizeof(bootId),"%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
    // PubSubClient's packet buffer includes the topic/header above payload bytes.
    if (!Mqtt::mqttClient.setBufferSize(9216)) lastError = "MQTT control buffer allocation failed";
    JsonDocument doc;
    if (!readDoc(TIMER_FILE,doc)) return;
    if (!doc.is<JsonArrayConst>() || doc.size() > 32) { lastError = "Invalid saved timers"; return; }
    bool seen[DailyTimers::PINS] = {};
    for (JsonVariantConst row : doc.as<JsonArrayConst>()) {
        unsigned on,off;
        if (!row["io"].is<unsigned>() || row["io"].as<unsigned>() >= DailyTimers::PINS || !row["enabled"].is<bool>() ||
            !DailyTimers::parseTime(row["tStart"],on) || !DailyTimers::parseTime(row["tStop"],off) || on == off) { lastError = "Invalid saved timer"; return; }
        unsigned io = row["io"];
        if (seen[io]) { lastError = "Duplicate saved timer GPIO"; return; }
        seen[io] = true;
    }
    for (JsonVariantConst row : doc.as<JsonArrayConst>()) {
        unsigned on,off; DailyTimers::parseTime(row["tStart"],on); DailyTimers::parseTime(row["tStop"],off);
        unsigned io = row["io"]; schedules[io] = {on,off,row["enabled"].as<bool>()}; exists[io] = true;
        if (schedules[io].enabled) writeOutput(io,false);
    }
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
        if (!DailyTimers::parseMessage(text,io,seconds) || !output(io)) { lastError = "Timer requires a configured available digital output"; dirty = true; return true; }
        DailyTimers::Schedule complete;
        if (pairs[io].receive(sunrise,seconds,millis(),complete) && !saveTimer(io,complete)) lastError = "Could not save complete timer";
    }
    dirty = true; return true;
}
void tick() {
    const time_t now = time(nullptr); const bool valid = now > 1510644967;
    for (unsigned io = 0; io < DailyTimers::PINS; ++io) if (exists[io]) runtimes[io].tick(schedules[io],now,valid,output(io),[io](bool active){ return writeOutput(io,active); });
    const bool connected = Mqtt::isEnabled && Mqtt::mqttClient.connected();
    if (connected && (!wasConnected || dirty || millis()-reportedAt >= 30000)) report(now,valid);
    if (connected && !wasConnected && Esp32::isConfigFromServer) Mqtt::mqttClient.publish("esp32/config",Esp32::DEVICE_NAME.c_str(),false);
    wasConnected = connected;
}
} // namespace IOControl
