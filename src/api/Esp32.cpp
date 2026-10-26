#include "Esp32.h"
#include "IOConfig.h"
#include "IOOutput.h"
#include "IOScenes.h"
#include "IOActivity.h"
#include <algorithm>
#include "SystemLog.h"
#include "devices/BMX280.h"
#include <esp_timer.h>
#include <freertos/semphr.h>
#include "JsonTools.h" // Added for JsonTools::getJsonString
#include <ArduinoJson.h>
//#include "..\Hockey.h" // Include for the global hockey object
#include "WifiManager.h" // For WifiManager object
#include "devices/Buzzer.h" // For Buzzer object
#include "Hourglass.h"    // For Hourglass object
#include "IPin.h"         // For Pin object
#include "Storage.h"      // For Storage functions
#include "Mqtt.h"         // For Mqtt functions/objects (like Mqtt::mqttClient, Mqtt::setup)
#include "MqttCommandRouter.h" // For MQTT command dispatch
#include <Wire.h>         // For I2C
#include <SPIFFS.h>       // For SPIFFS (though Esp32::setup handles begin)
#include <esp_log.h>      // For esp_log_level_set

// Define Namespace Variables
namespace Esp32 {
    bool isConfigFromServer = false;
    JsonDocument configJson_;
    String configString_ = "";
    bool spiffsMounted = false;
    String batteryText = "";
    float vBAT = 0.0f;
    byte vBATSampleSize = 5; 
    int battery_monitor_pin_ = 35; // Default from previous setup, will be overridden by JSON config if present
    float battery_divider_ratio_ = 2.0f; // Typical Feather VBAT divider is ~2:1
    float battery_calibration_factor_ = 1.0f;
    Esp32::Pin* ios[HUZZAH32]; 
    WifiManager wifiManager;
    Hourglass hourglass;
    ConfigManager configManager;

    bool buzzer_enabled_ = false;
    int configured_buzzer_pin_ = -1;
    int mqtt_data_interval_seconds_ = 5;

    // Tides config (defaults to Quebec City)
    float tides_lat_ = 46.8139f;
    float tides_lon_ = -71.2082f;

    std::vector<IO_Pin_Detail> configured_pins; // Definition for the vector
    static SemaphoreHandle_t ioMutex = xSemaphoreCreateRecursiveMutex();
    struct IOGuard {
        IOGuard() { xSemaphoreTakeRecursive(ioMutex, portMAX_DELAY); }
        ~IOGuard() { xSemaphoreGiveRecursive(ioMutex); }
    };
    static JsonDocument ioDocument;
    static String ioSession;
    static uint32_t ioRevision = 0;
    static uint32_t ioStateVersion = 1;
    static bool retiredPins[HUZZAH32] = {};
    static IOOutput::Timer ioTimers[HUZZAH32];
    static IOOutput::Timer ioOverrides[HUZZAH32];
    static String ioTimerScenes[HUZZAH32];
    static uint32_t ioTimerTraces[HUZZAH32] = {}, traceSequence = 0;
    static IOEvents::Journal<64> ioJournal;
    static IOActivity::Journal<64> activityJournal;
    static uint32_t nextTrace() { if (++traceSequence == 0) ++traceSequence; return traceSequence; }
    static void logActivity(IOActivity::Kind kind, const char* ref, const char* name, const char* message,
                            IOEvents::Source source, uint32_t trace = 0, const char* scene = nullptr, bool ok = true, int gpio = -1, double reading = NAN) {
        IOActivity::Event event; event.kind = kind; event.ok = ok; event.io.at = millis(); event.io.source = source; event.io.trace = trace; event.io.gpio = gpio;
        event.reading = reading;
        strncpy(event.ref, ref ? ref : "", sizeof(event.ref) - 1); strncpy(event.message, message ? message : "", sizeof(event.message) - 1);
        strncpy(event.io.label, name ? name : "", sizeof(event.io.label) - 1); strncpy(event.io.scene, scene ? scene : "", sizeof(event.io.scene) - 1);
        activityJournal.add(event);
    }
    static int inputLevels[HUZZAH32];
    static int inputCandidates[HUZZAH32];
    static uint32_t inputCandidateAt[HUZZAH32];
    struct PinUsage { int gpio; String label, mode; };
    static std::vector<PinUsage> pinUsages;
    static esp_timer_handle_t outputTimer;
    static portMUX_TYPE rebootMutex = portMUX_INITIALIZER_UNLOCKED;
    static bool rebootRequested = false;
    static uint32_t rebootRequestedAt = 0;
    struct AutomationState {
        String id, signature, message;
        IOAutomation::RuleState rule;
        IOAutomation::ScheduleState schedule;
        uint32_t lastRun = 0, count = 0;
        double value = NAN;
    };
    static std::vector<AutomationState> automationStates;
    struct AutomationRun { String name, scene, source, message, id; uint32_t at, trace; bool ok; };
    static std::vector<AutomationRun> automationRuns;
    static void reconcileAutomation() {
        std::vector<AutomationState> next;
        JsonVariantConst a = ioDocument["automation"];
        for (const char* key : {"schedules", "rules"}) for (JsonVariantConst item : a[key].as<JsonArrayConst>()) {
            AutomationState state; state.id = item["id"].as<String>();
            JsonDocument definition; definition["item"] = item; definition["paused"] = a["paused"]; definition["timezone"] = a["timezone"];
            for (JsonVariantConst scene : ioDocument["scenes"].as<JsonArrayConst>()) if (scene["id"] == item["scene_id"]) definition["scene"] = scene;
            if (!strcmp(key, "rules")) for (JsonVariantConst pin : ioDocument["io_pins"].as<JsonArrayConst>()) if (pin["gpio"] == item["gpio"]) definition["pin"] = pin;
            serializeJson(definition, state.signature);
            state.schedule.lastMinute = IOAutomation::clockValid(time(nullptr)) ? time(nullptr) / 60 : 0;
            for (const auto& previous : automationStates) if (previous.id == state.id) {
                if (previous.signature == state.signature) state = previous;
                else {
                    state.schedule.firedDay = previous.schedule.firedDay;
                    state.count = previous.count; state.lastRun = previous.lastRun;
                    state.rule.fired = previous.rule.fired; state.rule.lastRun = previous.rule.lastRun;
                }
            }
            next.push_back(state);
        }
        automationStates.swap(next);
    }
    static AutomationState* automationState(const String& id) {
        for (auto& state : automationStates) if (state.id == id) return &state;
        return nullptr;
    }
    struct AlertState { String id, signature; IOAlerts::State state; double value = NAN; uint32_t trace = 0; };
    static std::vector<AlertState> alertStates;
    static void reconcileAlerts() {
        std::vector<AlertState> next;
        for (JsonVariantConst item : ioDocument["alerts"].as<JsonArrayConst>()) {
            AlertState state; state.id = item["id"].as<String>(); JsonDocument definition; definition["item"] = item;
            if (item["source"] == "gpio") for (JsonVariantConst pin : ioDocument["io_pins"].as<JsonArrayConst>()) if (pin["gpio"] == item["gpio"]) definition["pin"] = pin;
            serializeJson(definition, state.signature);
            for (const auto& previous : alertStates) if (previous.id == state.id && previous.signature == state.signature) state = previous;
            next.push_back(state);
        }
        alertStates.swap(next);
    }

    static const IO_Pin_Detail* findPin(int gpio) {
        for (const auto& pin : configured_pins) if (pin.gpio == gpio) return &pin;
        return nullptr;
    }
    static bool outputPin(const IO_Pin_Detail* pin) {
        return pin && pin->mode_str == "OUTPUT" && pin->type_str == "DIGITAL";
    }
    static uint32_t logIO(const IO_Pin_Detail& pin, IOEvents::Source source, IOEvents::Kind kind,
                         int previous = -1, uint32_t duration = 0, const char* scene = nullptr, uint32_t trace = 0) {
        IOEvents::Event event;
        event.at = millis(); event.gpio = pin.gpio; event.source = source; event.kind = kind;
        event.previous = previous; event.duration = duration; event.output = outputPin(&pin);
        event.value = digitalRead(pin.gpio);
        event.active = event.output ? IOOutput::active(event.value == HIGH, pin.inverted) : event.value == HIGH;
        event.trace = trace;
        ioJournal.add(event, pin.label.c_str(), scene);
        IOActivity::Event entry; entry.io = ioJournal.at(ioJournal.size() - 1); activityJournal.add(entry);
        return ioJournal.latest();
    }
    static const char* reservedPin(int gpio) {
        for (const auto& usage : pinUsages) {
            if (usage.gpio == gpio && (usage.mode == "I2C" || usage.mode == "SERIAL")) return usage.label.c_str();
        }
        return nullptr;
    }
    bool isPinOutputAvailable(int gpio) {
        IOGuard guard;
        return outputPin(findPin(gpio)) && !reservedPin(gpio);
    }
    static String revision() { return ioSession + "." + String(ioRevision); }
    void registerPinUsage(int gpio, const char* label, const char* mode) {
        IOGuard guard;
        if (IOConfig::available(gpio)) pinUsages.push_back({gpio, label, mode});
    }
    void initializeModeInput(int gpio, byte mode) {
        IOGuard guard;
        if (!findPin(gpio)) pinMode(gpio, mode);
    }
    bool isModeInput(int gpio) {
        IOGuard guard;
        const auto* pin = findPin(gpio);
        return !pin || pin->mode_str != "OUTPUT";
    }
    void initializeModeOutput(int gpio, const char* label) {
        IOGuard guard;
        // A saved configuration owns the pin, including an intentional removal.
        if (findPin(gpio) || SPIFFS.exists(CONFIG_IO_FILENAME) || SPIFFS.exists(CONFIG_IO_FILENAME + ".bak")) return;
        JsonDocument defaults = ioDocument;
        JsonArray pins = defaults["io_pins"].is<JsonArray>() ? defaults["io_pins"].as<JsonArray>() : defaults["io_pins"].to<JsonArray>();
        JsonObject pin = pins.add<JsonObject>();
        pin["gpio"] = gpio; pin["label"] = label; pin["mode"] = "OUTPUT";
        pin["type"] = "DIGITAL"; pin["initial_state"] = "LOW";
        String json; serializeJson(defaults, json); applyIOConfiguration(json);
    }
    static void processOutputTimers(void*) {
        if (xSemaphoreTakeRecursive(ioMutex, 0) != pdTRUE) return;
        const uint32_t now = millis();
        for (const auto& pin : configured_pins) {
            if (outputPin(&pin) && ioOverrides[pin.gpio].expired(now)) {
                ioOverrides[pin.gpio].cancel(); ++ioStateVersion;
                logIO(pin, IOEvents::Source::TIMER, IOEvents::Kind::RESUMED);
            }
            if (outputPin(&pin) && ioTimers[pin.gpio].expired(now)) {
                int previous = getPinActive(pin.gpio);
                digitalWrite(pin.gpio, IOOutput::level(false, pin.inverted) ? HIGH : LOW);
                ioTimers[pin.gpio].cancel();
                ++ioStateVersion;
                logIO(pin, IOEvents::Source::TIMER, IOEvents::Kind::EXPIRED, previous, 0, ioTimerScenes[pin.gpio].c_str(), ioTimerTraces[pin.gpio]);
                ioTimerScenes[pin.gpio] = "";
                ioTimerTraces[pin.gpio] = 0;
            }
        }
        xSemaphoreGiveRecursive(ioMutex);
    }

    // Function Definitions
    void setVerboseLog() { esp_log_level_set("*", ESP_LOG_DEBUG); }

    float getCPUTemp() { return temperatureRead(); }

    int getCPUFreq() { return ESP.getCpuFreqMHz(); }

    int getRemainingHeap() { return ESP.getFreeHeap(); }

    bool validIO(int io) {
        Serial.print("valid io: ");
        Serial.println(io);
        if(io >= 0 && io < HUZZAH32 && ios[io] != nullptr) { // Added bounds check
            return true;  // GPIO 0 is a valid pin on ESP32, don't reject it
        }
        return false;
    }

    bool validIO(String ioStr) { return validIO(ioStr.toInt()); }

    void ioSwitch(int pin) { digitalWrite(pin, !digitalRead(pin)); }

    void ioBlink(int pin, int timeon, int timeoff, int iteration) {
        for (int i = 0; i < iteration; i++) {
            ioSwitch(pin);
            delay(timeon);
            ioSwitch(pin);
            delay(timeoff);
        }
    }

    Esp32::Pin* configPin(int gpio ,  const char* pinModeStr,  const char* label, bool isAnalog) {
       if(gpio >= 0 && gpio < HUZZAH32) { // Added bounds check
           if(ios[gpio] != nullptr) {
               delete ios[gpio];
               ios[gpio] = nullptr;
           }
           // Pin class is within Esp32 namespace, so Esp32:: qualifier is optional here.
           // However, ios array is Esp32::Pin*, so the type matches.
           ios[gpio] = new Pin(pinModeStr, gpio, label, isAnalog);
           return ios[gpio];
       } else {
           Serial.printf("Error: GPIO %d is out of bounds for configuration.\n", gpio);
           return nullptr;
       }
    }

    bool setPin(int gpio, bool high, IOEvents::Source source) {
        IOGuard guard;
        if (!isPinOutputAvailable(gpio)) return false;
        int previous = getPinActive(gpio);
        ioTimers[gpio].cancel();
        ioTimerScenes[gpio] = "";
        ioTimerTraces[gpio] = 0;
        digitalWrite(gpio, high ? HIGH : LOW);
        ++ioStateVersion;
        logIO(*findPin(gpio), source, IOEvents::Kind::COMMAND, previous, 0, nullptr, nextTrace());
        return true;
    }

    bool setPinActive(int gpio, bool active, uint32_t duration, IOEvents::Source source, const char* scene, uint32_t trace) {
        IOGuard guard;
        const auto* pin = findPin(gpio);
        if (!isPinOutputAvailable(gpio) || (duration && (!active || !IOOutput::validDuration(duration)))) return false;
        int previous = getPinActive(gpio);
        if (!trace) trace = nextTrace();
        digitalWrite(gpio, IOOutput::level(active, pin->inverted) ? HIGH : LOW);
        if (duration) ioTimers[gpio].start(millis(), duration);
        else ioTimers[gpio].cancel();
        ioTimerScenes[gpio] = duration && scene ? scene : "";
        ioTimerTraces[gpio] = duration ? trace : 0;
        ++ioStateVersion;
        logIO(*pin, source, duration ? IOEvents::Kind::PULSE : IOEvents::Kind::COMMAND, previous, duration, scene, trace);
        return true;
    }

    bool getPinActive(int gpio) {
        IOGuard guard;
        const auto* pin = findPin(gpio);
        return isPinOutputAvailable(gpio) && IOOutput::active(digitalRead(gpio) == HIGH, pin->inverted);
    }

    String setIOOutputJson(int gpio, bool value, bool logical, uint32_t duration) {
        IOGuard guard;
        if (!(logical ? setPinActive(gpio, value, duration) : setPin(gpio, value))) return "";
        JsonDocument result;
        result["status"] = "success"; result["gpio"] = gpio;
        result["value"] = digitalRead(gpio);
        result["active"] = getPinActive(gpio);
        result["remaining_ms"] = ioTimers[gpio].remaining(millis());
        result["event_id"] = ioJournal.latest();
        result["trace"] = ioJournal.at(ioJournal.size() - 1).trace;
        String json; serializeJson(result, json); return json;
    }

    bool togglePin(int gpio, IOEvents::Source source) {
        IOGuard guard;
        return setPinActive(gpio, !getPinActive(gpio), 0, source);
    }

    String runIOSceneJson(const String& id, const String& expectedRevision, IOEvents::Source source, bool* conflict, uint32_t trace) {
        IOGuard guard;
        if (!trace) trace = nextTrace();
        if (conflict) *conflict = false;
        JsonDocument result;
        const char* error = nullptr;
        std::vector<IOScenes::Action> actions;
        std::vector<int> skipped;
        std::string name;
        if (!expectedRevision.isEmpty() && expectedRevision != revision()) {
            if (conflict) *conflict = true;
            error = "Configuration modifiée ailleurs. Actualisez avant de lancer la scène.";
        } else if ((source == IOEvents::Source::RULE || source == IOEvents::Source::SCHEDULE) &&
                   IOOperator::sceneHold(ioDocument, id.c_str(), [](int gpio) { return ioOverrides[gpio].remaining(millis()); })) {
            error = "Priorité manuelle : scène ignorée, sans rattrapage.";
        } else error = IOScenes::plan(ioDocument, id.c_str(), isPinOutputAvailable, actions, skipped, name);
        if (name.empty()) { name = id == "all_off" ? "Tout OFF" : id.c_str(); for (JsonVariantConst scene : ioDocument["scenes"].as<JsonArrayConst>()) if (scene["id"].as<String>() == id) name = scene["name"] | id.c_str(); }
        logActivity(IOActivity::Kind::SCENE, id.c_str(), name.c_str(), error ? error : "Scène exécutée.", source, trace, name.c_str(), !error);
        result["trace"] = trace;
        if (error) { result["status"] = "error"; result["message"] = error; }
        else {
            result["status"] = "success"; result["id"] = id; result["name"] = name;
            JsonArray states = result["outputs"].to<JsonArray>();
            for (const auto& action : actions) {
                setPinActive(action.gpio, action.active, action.duration, source, name.c_str(), trace);
                JsonObject pin = states.add<JsonObject>();
                pin["gpio"] = action.gpio; pin["active"] = getPinActive(action.gpio);
                pin["value"] = digitalRead(action.gpio); pin["remaining_ms"] = ioTimers[action.gpio].remaining(millis());
            }
            JsonArray omitted = result["skipped"].to<JsonArray>();
            for (int gpio : skipped) omitted.add(gpio);
            result["event_id"] = ioJournal.latest();
        }
        String json; serializeJson(result, json); return json;
    }

    String getFriendlyName() {
        IOGuard guard; return ioDocument["device"]["name"] | DEVICE_NAME;
    }
    String setIOOverrideJson(int gpio, uint32_t duration, IOEvents::Source source) {
        IOGuard guard; JsonDocument result;
        if (!isPinOutputAvailable(gpio) || !IOOperator::validOverride(duration)) {
            result["status"] = "error"; result["message"] = "Priorité manuelle : sortie disponible, durée de 1 s à 24 h, ou 0 pour reprendre.";
        } else {
            if (duration) ioOverrides[gpio].start(millis(), duration); else ioOverrides[gpio].cancel();
            ++ioStateVersion;
            logIO(*findPin(gpio), source, duration ? IOEvents::Kind::OVERRIDE : IOEvents::Kind::RESUMED, -1, duration);
            result["status"] = "success"; result["gpio"] = gpio; result["override_remaining_ms"] = ioOverrides[gpio].remaining(millis());
        }
        String json; serializeJson(result, json); return json;
    }
    String stopIOZoneJson(const String& zone, const String& expectedRevision, uint32_t duration, bool* conflict) {
        IOGuard guard; JsonDocument result; std::vector<IOScenes::Action> actions; std::vector<int> skipped;
        if (conflict) *conflict = expectedRevision.isEmpty() || expectedRevision != revision();
        const char* error = expectedRevision.isEmpty() || expectedRevision != revision() ? "Configuration modifiée ailleurs. Actualisez avant de stopper la zone." :
            !duration || !IOOperator::validOverride(duration) ? "Choisissez une durée de priorité manuelle de 1 s à 24 h." :
            IOOperator::zonePlan(ioDocument, zone.c_str(), isPinOutputAvailable, actions, skipped);
        if (error) { result["status"] = "error"; result["message"] = error; }
        else {
            result["status"] = "success"; result["zone"] = zone; result["hold_ms"] = duration;
            JsonArray outputs = result["outputs"].to<JsonArray>();
            for (const auto& action : actions) {
                setIOOverrideJson(action.gpio, duration);
                setPinActive(action.gpio, false, 0, IOEvents::Source::WEB, zone.c_str());
                outputs.add(action.gpio);
            }
            JsonArray omitted = result["skipped"].to<JsonArray>(); for (int gpio : skipped) omitted.add(gpio);
        }
        String json; serializeJson(result, json); return json;
    }
    String resumeIOZoneJson(const String& zone, const String& expectedRevision, bool* conflict) {
        IOGuard guard; JsonDocument result; std::vector<IOScenes::Action> actions; std::vector<int> skipped;
        const bool stale = expectedRevision.isEmpty() || expectedRevision != revision();
        if (conflict) *conflict = stale;
        const char* error = stale ? "Configuration modifiée ailleurs. Actualisez avant de reprendre la zone." :
            IOOperator::zonePlan(ioDocument, zone.c_str(), isPinOutputAvailable, actions, skipped);
        if (error) { result["status"] = "error"; result["message"] = error; }
        else {
            result["status"] = "success"; result["zone"] = zone;
            JsonArray resumed = result["outputs"].to<JsonArray>();
            for (const auto& action : actions) {
                if (ioOverrides[action.gpio].remaining(millis())) {
                    setIOOverrideJson(action.gpio, 0);
                    resumed.add(action.gpio);
                }
            }
            JsonArray omitted = result["skipped"].to<JsonArray>(); for (int gpio : skipped) omitted.add(gpio);
        }
        String json; serializeJson(result, json); return json;
    }

    bool setAutomationPaused(bool paused, IOEvents::Source source) {
        IOGuard guard; JsonDocument next = ioDocument;
        if (next["automation"].isNull()) {
            next["automation"]["timezone"] = "America/Toronto";
            next["automation"]["schedules"].to<JsonArray>(); next["automation"]["rules"].to<JsonArray>();
        }
        next["automation"]["paused"] = paused;
        if (ioDocument["automation"]["paused"].is<bool>() && ioDocument["automation"]["paused"].as<bool>() == paused) return true;
        return saveAndApplyIOConfiguration(next, nullptr, source);
    }

    static double ruleReading(JsonVariantConst item) {
        int gpio = item["gpio"];
        const auto* pin = findPin(gpio);
        if (!pin || outputPin(pin) || reservedPin(gpio)) return NAN;
        if (pin->type_str != "ANALOG_INPUT") return digitalRead(gpio);
        if (IOConfig::adc2(gpio) && WiFi.getMode() != WIFI_OFF) return NAN;
        double raw = analogRead(gpio);
        for (JsonVariantConst configured : ioDocument["io_pins"].as<JsonArrayConst>())
            if (configured["gpio"].as<int>() == gpio && !configured["calibration"].isNull()) return IOCalibration::convert(raw, configured["calibration"]);
        return raw;
    }
    static double alertReading(JsonVariantConst item) {
        if (item["source"] == "gpio") return ruleReading(item);
        if (!SystemLog::getSensorCount() || millis() - SystemLog::getLastSensorMillis() > static_cast<uint32_t>(std::max(15, mqtt_data_interval_seconds_ * 3)) * 1000) return NAN;
        return BMX280::isSuccessfullyInitialized() ? BMX280::getTemperature() : NAN;
    }
    void processIOAlerts() {
        static uint32_t last = 0; const uint32_t now = millis(); if (now - last < 250) return; last = now;
        IOGuard guard;
        for (JsonVariantConst item : ioDocument["alerts"].as<JsonArrayConst>()) for (auto& state : alertStates) if (state.id == item["id"].as<String>()) {
            state.value = alertReading(item);
            if (!(item["enabled"] | false)) { state.state.unavailable(); continue; }
            auto change = IOAlerts::evaluate(item, state.state, state.value, now);
            if (change == IOAlerts::Change::ACTIVE) state.trace = nextTrace();
            if (change != IOAlerts::Change::NONE) logActivity(change == IOAlerts::Change::ACTIVE ? IOActivity::Kind::ALERT_ACTIVE : IOActivity::Kind::ALERT_CLEAR,
                state.id.c_str(), item["name"] | "", change == IOAlerts::Change::ACTIVE ? "Seuil dépassé après stabilité." : "Retour dans la plage après stabilité.", IOEvents::Source::INPUT_SIGNAL, state.trace, nullptr, true, item["source"] == "gpio" ? item["gpio"].as<int>() : -1, state.value);
        }
    }
    String getIOAlertsStatusJson() {
        IOGuard guard; JsonDocument doc; doc["_revision"] = revision(); doc["session"] = ioSession; doc["now_ms"] = millis();
        JsonArray items = doc["items"].to<JsonArray>();
        for (JsonVariantConst alert : ioDocument["alerts"].as<JsonArrayConst>()) for (const auto& state : alertStates) if (state.id == alert["id"].as<String>()) {
            JsonObject item = items.add<JsonObject>(); item["id"] = state.id; item["active"] = (alert["enabled"] | false) && state.state.active && std::isfinite(state.value);
            item["state"] = !(alert["enabled"] | false) ? "disabled" : !std::isfinite(state.value) ? "unavailable" : state.state.active ? "active" : state.state.candidate ? "pending" : "normal";
            if (std::isfinite(state.value)) item["value"] = state.value; else item["value"] = nullptr;
            item["changed_at_ms"] = state.state.changedAt;
            uint32_t stable = alert["stable_ms"] | 2000U, elapsed = millis() - state.state.candidateAt;
            item["stable_remaining_ms"] = state.state.initialized && state.state.candidate != state.state.active && elapsed < stable ? stable - elapsed : 0;
        }
        String json; serializeJson(doc, json); return json;
    }
    String previewIOAutomationJson(const JsonDocument& config, const String& id) {
        IOGuard guard; JsonDocument result; const char* error = IOConfig::validate(config);
        JsonVariantConst item; bool rule = false;
        for (const char* key : {"schedules", "rules"}) for (JsonVariantConst candidate : config["automation"][key].as<JsonArrayConst>())
            if (candidate["id"].as<String>() == id) { item = candidate; rule = !strcmp(key, "rules"); }
        if (!error && item.isNull()) error = "Automatisme introuvable dans le brouillon.";
        if (!error && rule) {
            JsonVariantConst configured;
            for (JsonVariantConst pin : config["io_pins"].as<JsonArrayConst>()) if (pin["gpio"] == item["gpio"]) configured = pin;
            JsonVariantConst current;
            for (JsonVariantConst pin : ioDocument["io_pins"].as<JsonArrayConst>()) if (pin["gpio"] == item["gpio"]) current = pin;
            String expected, actual; serializeJson(configured, expected); serializeJson(current, actual);
            if (expected != actual) error = "La source a changé. Rechargez sa configuration avant l'aperçu.";
        }
        if (error) { result["status"] = "error"; result["message"] = error; }
        else {
            result["status"] = "success"; result["simulation"] = true; result["requires_new_edge"] = rule;
            result["id"] = id; result["paused"] = config["automation"]["paused"]; result["enabled"] = item["enabled"];
            std::vector<IOScenes::Action> actions; std::vector<int> skipped; std::string name;
            const char* unavailable = IOScenes::plan(config, item["scene_id"] | "", isPinOutputAvailable, actions, skipped, name);
            result["targets_available"] = !unavailable; if (unavailable) result["target_reason"] = unavailable;
            result["override_remaining_ms"] = IOOperator::sceneHold(config, item["scene_id"] | "", [](int gpio) { return ioOverrides[gpio].remaining(millis()); });
            JsonArray outputs = result["outputs"].to<JsonArray>();
            for (const auto& action : actions) { JsonObject output = outputs.add<JsonObject>(); output["gpio"] = action.gpio; output["active"] = action.active; output["duration_ms"] = action.duration; }
            if (rule) {
                if (!item["window"].isNull()) { time_t now = time(nullptr); result["clock_valid"] = IOAutomation::clockValid(now); result["window_allowed"] = IOAutomation::windowAllowed(item, now, config["automation"]["timezone"] | "America/Toronto"); }
                double value = ruleReading(item);
                if (std::isfinite(value)) { result["value"] = value; result["condition_now"] = IOAutomation::condition(item, value, false); }
                else { result["value"] = nullptr; result["condition_now"] = nullptr; }
                result["stable_ms"] = item["stable_ms"]; result["cooldown_ms"] = item["cooldown_ms"];
            } else {
                time_t now = time(nullptr); result["clock_valid"] = IOAutomation::clockValid(now);
                result["next_epoch"] = IOAutomation::clockValid(now) ? static_cast<int64_t>(IOAutomation::next(item, now, config["automation"]["timezone"] | "America/Toronto")) : 0;
            }
        }
        String json; serializeJson(result, json); return json;
    }
    void processIOAutomation() {
        static uint32_t last = 0;
        if (millis() - last < 250) return;
        last = millis(); IOGuard guard;
        JsonVariantConst a = ioDocument["automation"];
        bool paused = a.isNull() || (a["paused"] | true);
        const char* zone = a["timezone"] | "America/Toronto";
        for (const char* key : {"schedules", "rules"}) for (JsonVariantConst item : a[key].as<JsonArrayConst>()) {
            auto* state = automationState(item["id"].as<String>()); if (!state) continue;
            bool isRule = !strcmp(key, "rules");
            if (isRule) state->value = ruleReading(item);
            if (paused || !(item["enabled"] | false)) continue;
            IOAutomation::Attempt attempt = isRule ? IOAutomation::attempt(item, state->rule, state->value, millis(), time(nullptr), zone) : IOAutomation::Attempt::NONE;
            bool trigger = isRule ? attempt != IOAutomation::Attempt::NONE : IOAutomation::due(item, state->schedule, time(nullptr), zone);
            if (!trigger) continue;
            auto source = isRule ? IOEvents::Source::RULE : IOEvents::Source::SCHEDULE;
            uint32_t trace = nextTrace();
            JsonDocument result;
            if (attempt == IOAutomation::Attempt::OUTSIDE_WINDOW || attempt == IOAutomation::Attempt::CLOCK_UNAVAILABLE) { result["status"] = "error"; result["message"] = attempt == IOAutomation::Attempt::OUTSIDE_WINDOW ? "Hors plage horaire : franchissement ignoré, sans rattrapage." : "Horloge indisponible : franchissement ignoré, sans rattrapage."; }
            const bool blocked = attempt == IOAutomation::Attempt::OUTSIDE_WINDOW || attempt == IOAutomation::Attempt::CLOCK_UNAVAILABLE;
            logActivity(isRule ? IOActivity::Kind::RULE : IOActivity::Kind::SCHEDULE, item["id"] | "", item["name"] | "",
                blocked ? result["message"] | "" : isRule ? "Condition stable atteinte." : "Heure programmée atteinte.", source, trace, item["scene_id"] | "", !blocked, isRule ? item["gpio"].as<int>() : -1, isRule ? state->value : NAN);
            if (!blocked) deserializeJson(result, runIOSceneJson(item["scene_id"].as<String>(), "", source, nullptr, trace));
            state->lastRun = millis(); ++state->count;
            bool ok = result["status"] == "success";
            state->message = ok ? "Exécutée" : result["message"].as<String>();
            if (automationRuns.size() >= 32) automationRuns.erase(automationRuns.begin());
            automationRuns.push_back({item["name"].as<String>(), item["scene_id"].as<String>(), IOEvents::name(source), state->message, item["id"].as<String>(), state->lastRun, trace, ok});
        }
    }
    String getIOAutomationStatusJson() {
        IOGuard guard;
        JsonDocument result; JsonVariantConst a = ioDocument["automation"];
        const char* zone = a["timezone"] | "America/Toronto";
        time_t now = time(nullptr); bool valid = IOAutomation::clockValid(now);
        result["_revision"] = revision(); result["paused"] = a.isNull() || (a["paused"] | true);
        result["timezone"] = zone; result["clock_valid"] = valid; result["now_ms"] = millis();
        result["epoch"] = valid ? static_cast<int64_t>(now) : 0;
        if (valid) { tm local = IOAutomation::calendar(now, zone); char date[24]; strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local); result["local_time"] = date; }
        JsonArray statuses = result["items"].to<JsonArray>();
        for (const char* key : {"schedules", "rules"}) for (JsonVariantConst item : a[key].as<JsonArrayConst>()) {
            auto* state = automationState(item["id"].as<String>()); if (!state) continue;
            JsonObject status = statuses.add<JsonObject>(); status["id"] = state->id; status["count"] = state->count;
            status["last_run_ms"] = state->lastRun; status["message"] = state->message;
            uint32_t held = IOOperator::sceneHold(ioDocument, item["scene_id"] | "", [](int gpio) { return ioOverrides[gpio].remaining(millis()); });
            status["override_remaining_ms"] = held;
            bool rule = !strcmp(key, "rules");
            bool windowAllowed = !rule || IOAutomation::windowAllowed(item, now, zone);
            if (rule && !item["window"].isNull()) status["window_allowed"] = windowAllowed;
            status["state"] = result["paused"].as<bool>() ? "paused" : !(item["enabled"] | false) ? "disabled" : !rule && !valid ? "clock_unavailable" : rule && !std::isfinite(state->value) ? "input_unavailable" : rule && !state->rule.initialized ? "initializing" : rule && state->rule.condition ? "condition_active" : "armed";
            if (rule && !result["paused"].as<bool>() && (item["enabled"] | false) && !windowAllowed && std::isfinite(state->value)) status["state"] = valid ? "outside_window" : "clock_unavailable";
            if (rule) {
                if (std::isfinite(state->value)) status["value"] = state->value; else status["value"] = nullptr;
                status["condition"] = state->rule.condition; status["candidate"] = state->rule.candidate;
                uint32_t stable = item["stable_ms"] | 2000U, elapsed = millis() - state->rule.candidateAt;
                status["stable_remaining_ms"] = state->rule.initialized && state->rule.candidate != state->rule.condition && elapsed < stable ? stable - elapsed : 0;
                uint32_t cooldown = item["cooldown_ms"] | 60000U, since = millis() - state->rule.lastRun;
                status["cooldown_remaining_ms"] = state->rule.fired && since < cooldown ? cooldown - since : 0;
                status["condition_now"] = std::isfinite(state->value) ? IOAutomation::condition(item, state->value, state->rule.condition) : false;
            }
            else status["next_epoch"] = !result["paused"].as<bool>() && (item["enabled"] | false) && valid ? static_cast<int64_t>(IOAutomation::next(item, now, zone, state->schedule.firedDay)) : 0;
        }
        JsonArray runs = result["runs"].to<JsonArray>();
        for (auto it = automationRuns.rbegin(); it != automationRuns.rend(); ++it) {
            JsonObject entry = runs.add<JsonObject>(); entry["name"] = it->name; entry["scene_id"] = it->scene; entry["source"] = it->source; entry["at_ms"] = it->at; entry["ok"] = it->ok; entry["message"] = it->message;
            entry["id"] = it->id; entry["trace"] = it->trace;
        }
        String json; serializeJson(result, json); return json;
    }

    String getIOEventsJsonString(uint32_t after, const String& session) {
        IOGuard guard;
        JsonDocument doc;
        bool reset = (!session.isEmpty() && session != ioSession) || after > ioJournal.latest();
        if (reset) after = 0;
        doc["session"] = ioSession; doc["now_ms"] = millis();
        doc["latest_id"] = ioJournal.latest(); doc["capacity"] = 64; doc["reset"] = reset;
        doc["lost"] = after > 0 && ioJournal.oldest() > after + 1 ? ioJournal.oldest() - after - 1 : 0;
        JsonArray events = doc["events"].to<JsonArray>();
        for (size_t i = 0; i < ioJournal.size(); ++i) {
            const auto& event = ioJournal.at(i);
            if (event.id <= after) continue;
            JsonObject item = events.add<JsonObject>();
            item["id"] = event.id; item["at_ms"] = event.at; item["gpio"] = event.gpio;
            item["label"] = event.label; item["source"] = IOEvents::name(event.source);
            item["kind"] = IOEvents::name(event.kind); item["value"] = event.value;
            item["active"] = event.active; item["output"] = event.output;
            item["trace"] = event.trace;
            if (event.previous >= 0) item["previous"] = event.previous;
            if (event.duration) item["duration_ms"] = event.duration;
            if (*event.scene) item["scene"] = event.scene;
        }
        String json; serializeJson(doc, json); return json;
    }
    String getIOActivityJson(uint32_t after, const String& session) {
        IOGuard guard; JsonDocument doc; bool reset = (!session.isEmpty() && session != ioSession) || after > activityJournal.latest(); if (reset) after = 0;
        doc["session"] = ioSession; doc["now_ms"] = millis(); doc["latest_id"] = activityJournal.latest(); doc["capacity"] = 64; doc["reset"] = reset;
        doc["lost"] = after > 0 && activityJournal.oldest() > after + 1 ? activityJournal.oldest() - after - 1 : 0;
        JsonArray events = doc["events"].to<JsonArray>();
        for (size_t i = 0; i < activityJournal.size(); ++i) {
            const auto& event = activityJournal.at(i); const auto& e = event.io; if (e.id <= after) continue;
            JsonObject item = events.add<JsonObject>(); item["id"] = e.id; item["at_ms"] = e.at; item["trace"] = e.trace;
            item["category"] = IOActivity::name(event.kind); item["source"] = IOEvents::name(e.source); item["label"] = e.label; item["ok"] = event.ok; if (e.gpio >= 0) item["gpio"] = e.gpio;
            if (std::isfinite(event.reading)) item["reading"] = event.reading;
            if (event.kind == IOActivity::Kind::GPIO) { item["gpio"] = e.gpio; item["kind"] = IOEvents::name(e.kind); item["output"] = e.output; item["active"] = e.active; item["value"] = e.value; if (e.duration) item["duration_ms"] = e.duration; }
            if (*e.scene) item["scene"] = e.scene; if (*event.ref) item["ref"] = event.ref; if (*event.message) item["message"] = event.message;
        }
        String json; serializeJson(doc, json); return json;
    }

    void reboot() {
        Serial.println(F("!!!  Rebooting device..."));
        delay(1500);
        ESP.restart();
    }
    void requestReboot() {
        portENTER_CRITICAL(&rebootMutex);
        rebootRequestedAt = millis(); rebootRequested = true;
        portEXIT_CRITICAL(&rebootMutex);
    }

    int i2cScanner() {
        byte count = 0;
        Serial.print (" -- I2C Scanning -- \n");
        Wire.begin(); // Ensure Wire is initialized for I2C
        for (byte i = 8; i < 120; i++) {
            Wire.beginTransmission (i);
            if (Wire.endTransmission () == 0) {
                Serial.print ("Found address: ");
                Serial.print (i, DEC);
                Serial.print (" (0x");
                Serial.print (i, HEX);
                Serial.println (")");
                count++;
            }
        }
        Serial.println ("Scanning Done.");
        Serial.print ("Found "); Serial.print (count, DEC); Serial.println (" device(s).");
        return count;
    }

    float getBatteryVoltage() {
        if (battery_monitor_pin_ == -1) {
            Serial.println(F("Error: Battery monitor pin not configured. Cannot read voltage."));
            vBAT = 0.0f;
            return vBAT;
        }

        uint32_t battery_pin_mv = analogReadMilliVolts(battery_monitor_pin_);
        if (battery_pin_mv == 0) {
            vBAT = 0.0f;
            return vBAT;
        }

        vBAT = (battery_pin_mv / 1000.0f) * battery_divider_ratio_ * battery_calibration_factor_;
        return vBAT;
    }

    float getBattRemaining(bool print) {
        float voltage_sum = 0.0f;
        for (byte i = 0; i < vBATSampleSize; i++) {
            voltage_sum += getBatteryVoltage();
        }
        float average_vBAT = voltage_sum / vBATSampleSize;
        vBAT = average_vBAT;
        if(print) {
            batteryText = String(vBAT, 2);
            Serial.print("Battery Voltage: ");
            Serial.print(batteryText);
            Serial.println("V");
        }
        return vBAT;
    }

    void executeJsonConfig() {
        Serial.println("\nExecuting Config!");

        wifiManager.setup(true, configJson_["ssid"] | "", configJson_["pass"] | "");

        long gmt_offset = configJson_["gmtOffset_sec"] | -18000L;
        int dst_offset = configJson_["daylightOffset_sec"] | 3600;
        hourglass.setTimezone(gmt_offset, dst_offset);

        if(Esp32::wifiManager.isConnected()) {
            if(hourglass.setupTimeSync()) hourglass.getDateTimeString(true);
        }

        Esp32::buzzer_enabled_ = configJson_["buzzer_enabled"] | false;
        Esp32::configured_buzzer_pin_ = configJson_["buzzer_pin"] | 14;
        Esp32::battery_divider_ratio_ = configJson_["battery_divider_ratio"] | 2.0f;
        if (Esp32::battery_divider_ratio_ <= 0.0f) Esp32::battery_divider_ratio_ = 2.0f;
        Esp32::battery_calibration_factor_ = configJson_["battery_calibration_factor"] | 1.0f;
        if (Esp32::battery_calibration_factor_ <= 0.0f) Esp32::battery_calibration_factor_ = 1.0f;

        if (Esp32::buzzer_enabled_ && Esp32::configured_buzzer_pin_ != -1) {
            BuzzerModule::init(Esp32::configured_buzzer_pin_); // Changed to BuzzerModule
        } else {
            BuzzerModule::init(-1); // Changed to BuzzerModule
            Serial.println(F("Buzzer: Disabled or pin not set in config."));
        }

        Mqtt::isEnabled = configJson_["isMqtt"] | false;
        Esp32::isConfigFromServer = configJson_["isConfigFromServer"] | false;
        Mqtt::port = configJson_["mqttport"] | 1883;
        Esp32::mqtt_data_interval_seconds_ = configJson_["mqttDataIntervalSec"] | 5;
        if (Esp32::mqtt_data_interval_seconds_ < 1) {
            Esp32::mqtt_data_interval_seconds_ = 1;
        }
        Serial.printf("Esp32: MQTT data send interval set to %d seconds.\n", Esp32::mqtt_data_interval_seconds_);

        // Tides configuration
        Esp32::tides_lat_ = configJson_["tides_lat"] | 46.8139f;
        Esp32::tides_lon_ = configJson_["tides_lon"] | -71.2082f;
        Serial.printf("Esp32: Tides location: lat=%.4f, lon=%.4f\n", Esp32::tides_lat_, Esp32::tides_lon_);

        if(Mqtt::isEnabled) {
            Mqtt::mqttClient.setCallback(MqttCommandRouter::handleIncoming);
            if (!Mqtt::setup(DEVICE_NAME, configJson_)) Serial.println(F("MQTT setup failed\n"));
            else Serial.println(F("MQTT setup initiated\n"));
        }
    }

    bool loadConfig(bool doExecuteConfig, JsonDocument* returnDoc) {
        if (!Esp32::spiffsMounted) {
            Serial.println(F("Error: SPIFFS not mounted. Cannot load config."));
            return false;
        }

        if (!configManager.loadConfig(Esp32::CONFIG_FILENAME)) {
            return false;
        }

        configJson_ = configManager.getConfig();
        configString_ = configManager.getConfigString(true);

        if (returnDoc) {
            *returnDoc = configJson_;
        }

        if (doExecuteConfig) executeJsonConfig();
        return true;
    }

    bool saveConfig(JsonDocument& config, bool do_reboot) {
        if (!Esp32::spiffsMounted) {
            Serial.println(F("Error: SPIFFS not mounted. Cannot save config."));
            return false;
        }

        if (!configManager.saveConfig(Esp32::CONFIG_FILENAME, config)) {
            Serial.println(F("Error: ConfigManager failed to save config."));
            return false;
        }

        configJson_ = config;
        configString_ = configManager.getConfigString(true);

        if (do_reboot) Esp32::reboot();
        else loadConfig(false);
        return true;
    }

    void setup() {
        ioSession = String(esp_random(), HEX);
        ioDocument["io_pins"].to<JsonArray>();
        registerPinUsage(1, "Console série TX", "SERIAL");
        registerPinUsage(3, "Console série RX", "SERIAL");
        registerPinUsage(21, "I²C · SDA", "I2C");
        registerPinUsage(22, "I²C · SCL", "I2C");
        registerPinUsage(battery_monitor_pin_, "Mesure de batterie", "ANALOG_INPUT");
        esp_timer_create_args_t timerArgs = {};
        timerArgs.callback = processOutputTimers;
        timerArgs.name = "io_outputs";
        ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &outputTimer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(outputTimer, 20000));
        for (int i = 0; i < HUZZAH32; ++i) { // Initialize ios array
            ios[i] = nullptr;
            inputLevels[i] = -1;
            inputCandidates[i] = -1;
        }

        Serial.println("\nEsp32 setup\nStarting internal SPIFFS filesystem");

        const int EEPROM_SIZE_FOR_APP = 64;
        EEPROM.begin(EEPROM_SIZE_FOR_APP);
        Serial.printf("EEPROM Initialized with size for app: %d bytes\n", EEPROM_SIZE_FOR_APP);



        // Esp32::battery_monitor_pin_ = 35; // Already initialized at definition
        analogReadResolution(12);
        analogSetPinAttenuation(battery_monitor_pin_, ADC_11db);

        if(!SPIFFS.begin(false)) {
            Serial.println(F("SPIFFS mount failed; filesystem left intact for recovery."));
            ioBlink(LED_BUILTIN,100, 100, 8);
            spiffsMounted = false;
        } else {
            Serial.println("SPIFFS mounted successfully");
            spiffsMounted = true;
            Storage::listDir("/", 0); // List only root directory contents
            Esp32::loadAndApplyIOConfig(); // Load and apply I/O config

            if(!loadConfig(false, &configJson_)) { // Pass address of global configJson_
                JsonDocument configDoc; // Local temp doc for default creation
                configDoc["isMqtt"] = false;
                configDoc["isConfigFromServer"] = false;
                configDoc["ssid"] = "";
                configDoc["pass"] ="";
                configDoc["mqttport"] = 1883;
                configDoc["mqtturl"] = "";
                configDoc["profileName"] = "default_ESP32";
                configDoc["gmtOffset_sec"] = -18000;
                configDoc["daylightOffset_sec"] = 3600;
                configDoc["buzzer_enabled"] = false;
                configDoc["buzzer_pin"] = 14;
                configDoc["mqttDataIntervalSec"] = 5;
                configDoc["tides_lat"] = 46.8139f;
                configDoc["tides_lon"] = -71.2082f;
                configDoc["tides_api_key"] = "";
                Serial.println("setup -> Could not read Config file -> initializing new file");

                saveConfig(configDoc, false); // Pass local temp doc, saveConfig updates global configJson_
            } else {
                Serial.println("ConfigJson was retrieved.");
            }
            executeJsonConfig();  // Execute with the loaded or newly created (and now global) configJson_
        }
        i2cScanner();
    }

    void loop() {
        portENTER_CRITICAL(&rebootMutex);
        bool restart = rebootRequested && millis() - rebootRequestedAt >= 500;
        if (restart) rebootRequested = false;
        portEXIT_CRITICAL(&rebootMutex);
        if (restart) reboot();
        wifiManager.loop();
        processIOAutomation();
        processIOAlerts();
        if (Esp32::buzzer_enabled_) {
            BuzzerModule::loop(); // Changed to BuzzerModule
        }
        if(Mqtt::isEnabled) Mqtt::loop();
        static uint32_t lastInputPoll = 0;
        if (millis() - lastInputPoll >= 10) {
            lastInputPoll = millis();
            IOGuard guard;
            for (const auto& pin : configured_pins) {
                if (outputPin(&pin) || pin.type_str != "DIGITAL" || reservedPin(pin.gpio)) continue;
                int level = digitalRead(pin.gpio);
                if (inputCandidates[pin.gpio] != level) {
                    inputCandidates[pin.gpio] = level;
                    inputCandidateAt[pin.gpio] = millis();
                    continue;
                }
                if (millis() - inputCandidateAt[pin.gpio] < 30) continue;
                if (inputLevels[pin.gpio] >= 0 && level != inputLevels[pin.gpio]) {
                    logIO(pin, IOEvents::Source::INPUT_SIGNAL, IOEvents::Kind::INPUT_CHANGED, inputLevels[pin.gpio]);
                    ++ioStateVersion;
                }
                inputLevels[pin.gpio] = level;
            }
        }
        // The main loop owns MQTT. HTTP and timer tasks only mark the state dirty.
        static uint32_t publishedVersion = 0, lastPublished = 0;
        if (Mqtt::getState() == Mqtt::MqttState::CONNECTED) {
            JsonDocument status;
            uint32_t version;
            bool retired[HUZZAH32] = {};
            {
                IOGuard guard;
                version = ioStateVersion;
                if (millis() - lastPublished < 200) return;
                if (version == publishedVersion && millis() - lastPublished < 5000) return;
                deserializeJson(status, getIOStatusJsonString());
                for (int gpio = 0; gpio < HUZZAH32; ++gpio) {
                    retired[gpio] = retiredPins[gpio];
                    retiredPins[gpio] = false;
                }
            }
            Mqtt::publishIOHomeAssistantDiscovery(status);
            bool ok = true;
            JsonDocument automationStatus;
            {
                IOGuard guard;
                automationStatus["paused"] = ioDocument["automation"].isNull() || (ioDocument["automation"]["paused"] | true);
                automationStatus["clock_valid"] = IOAutomation::clockValid(time(nullptr));
            }
            String automationMessage; serializeJson(automationStatus, automationMessage);
            if (!Mqtt::publish(String("esp32/" + DEVICE_NAME + "/automation/state").c_str(), automationMessage.c_str(), true)) ok = false;
            for (int gpio = 0; gpio < HUZZAH32; ++gpio) if (retired[gpio]) {
                String topic = "esp32/" + DEVICE_NAME + "/io/" + String(gpio) + "/state";
                if (!Mqtt::publish(topic.c_str(), "", true)) {
                    IOGuard guard; retiredPins[gpio] = true; ok = false;
                }
            }
            for (JsonObject pin : status["statuses"].as<JsonArray>()) {
                String topic = "esp32/" + DEVICE_NAME + "/io/" + pin["gpio"].as<String>() + "/state";
                String message; serializeJson(pin, message);
                if (!Mqtt::publish(topic.c_str(), message.c_str(), true)) ok = false;
            }
            if (ok) publishedVersion = version;
            lastPublished = millis();
        }
    }

    // I/O Configuration Core Logic Implementations
    void applyIOConfiguration(const String& jsonConfigString, IOEvents::Source source) {
        IOGuard guard;
        JsonDocument doc;
        if (deserializeJson(doc, jsonConfigString)) {
            Serial.println(F("Esp32: Invalid I/O JSON; keeping the active configuration."));
            return;
        }
        if (const char* error = IOConfig::validate(doc)) {
            Serial.println(error);
            return;
        }

        // Keep existing output levels when editing names, graphs, or startup defaults.
        std::vector<IO_Pin_Detail> next;
        for (JsonObjectConst pin : doc["io_pins"].as<JsonArrayConst>()) {
            int gpio = pin["gpio"];
            String label = pin["label"];
            String mode = pin["mode"] | "INPUT";
            String type = pin["type"] | "DIGITAL";
            String initial = pin["initial_state"] | "";
            bool inverted = pin["inverted"] | false;
            if (IOConfig::inputOnly(gpio)) mode = "INPUT";
            byte actual = INPUT;
            const char* pinModeName = "IN";
            if (mode == "OUTPUT") { actual = OUTPUT; pinModeName = "OUT"; }
            else if (mode == "INPUT_PULLUP") { actual = INPUT_PULLUP; pinModeName = "INPULL"; }
            else if (mode == "INPUT_PULLDOWN") { actual = INPUT_PULLDOWN; pinModeName = "INPULLD"; }

            const IO_Pin_Detail* previous = nullptr;
            for (const auto& old : configured_pins) if (old.gpio == gpio) { previous = &old; break; }
            bool sameRole = previous && previous->mode_str == mode && previous->type_str == type;
            bool modified = !previous || previous->label != label || !sameRole ||
                            previous->initial_state_str != initial || previous->inverted != inverted;
            if (sameRole && ios[gpio]) {
                ios[gpio]->config.label = label;
                if (actual == OUTPUT && !reservedPin(gpio) && previous->inverted != inverted) {
                    bool active = IOOutput::active(digitalRead(gpio) == HIGH, previous->inverted);
                    digitalWrite(gpio, IOOutput::level(active, inverted) ? HIGH : LOW);
                }
            } else {
                ioTimers[gpio].cancel();
                ioOverrides[gpio].cancel();
                ioTimerScenes[gpio] = ""; ioTimerTraces[gpio] = 0;
                // Explicit startup values remain electrical LOW/HIGH for old configurations.
                if (!reservedPin(gpio)) {
                    if (actual == OUTPUT) digitalWrite(gpio, initial.isEmpty() ? (inverted ? HIGH : LOW) : (initial == "HIGH" ? HIGH : LOW));
                    configPin(gpio, pinModeName, label.c_str(), type == "ANALOG_INPUT");
                }
            }
            next.emplace_back(gpio, label, mode, actual, type, initial, pin["graph"] | false, inverted);
            if (!reservedPin(gpio) && type == "DIGITAL" && modified)
                logIO(next.back(), source, source == IOEvents::Source::BOOT ? IOEvents::Kind::STARTUP : IOEvents::Kind::CONFIGURED);
            if (!sameRole) { inputLevels[gpio] = -1; inputCandidates[gpio] = -1; }
        }
        for (const auto& old : configured_pins) {
            bool retained = false;
            for (const auto& pin : next) if (pin.gpio == old.gpio) { retained = true; break; }
            if (!retained) {
                if (!reservedPin(old.gpio) && old.type_str == "DIGITAL")
                    logIO(old, source, IOEvents::Kind::REMOVED);
                if (!reservedPin(old.gpio)) {
                    if (outputPin(&old)) digitalWrite(old.gpio, IOOutput::level(false, old.inverted) ? HIGH : LOW);
                    pinMode(old.gpio, INPUT);
                }
                ioTimers[old.gpio].cancel();
                ioOverrides[old.gpio].cancel();
                ioTimerScenes[old.gpio] = ""; ioTimerTraces[old.gpio] = 0;
                retiredPins[old.gpio] = true;
                delete ios[old.gpio];
                ios[old.gpio] = nullptr;
            }
        }
        configured_pins.swap(next);
        ioDocument = doc;
        ioDocument.remove("_revision");
        reconcileAutomation();
        reconcileAlerts();
        ++ioRevision;
        ++ioStateVersion;
        Serial.printf("Esp32: Applied I/O configuration for %u pin(s).\n", configured_pins.size());
    }

    bool saveAndApplyIOConfiguration(const JsonDocument& doc, bool* conflict, IOEvents::Source source) {
        IOGuard guard;
        if (conflict) *conflict = false;
        if (!doc["_revision"].isNull() && doc["_revision"].as<String>() != revision()) {
            if (conflict) *conflict = true;
            return false;
        }
        if (IOConfig::validate(doc)) return false;
        JsonDocument clean = doc;
        clean.remove("_revision");
        String json;
        serializeJson(clean, json);
        if (!Storage::writeConfigFile(CONFIG_IO_FILENAME, json)) return false;
        applyIOConfiguration(json, source);
        return true;
    }

    String getIOConfigurationJsonString() {
        IOGuard guard;
        JsonDocument doc = ioDocument;
        doc["_revision"] = revision();
        String json; serializeJson(doc, json); return json;
    }

    bool mergeLegacyIOConfiguration(const JsonDocument& incoming) {
        IOGuard guard;
        if (!incoming.is<JsonArrayConst>()) return saveAndApplyIOConfiguration(incoming, nullptr, IOEvents::Source::MQTT);
        // Old MQTT arrays are patches. Preserve every other pin and document field.
        JsonDocument merged = ioDocument;
        JsonArray pins = merged["io_pins"].as<JsonArray>();
        bool seen[40] = {};
        for (JsonVariantConst item : incoming.as<JsonArrayConst>()) {
            if (!item.is<JsonObjectConst>() || !item["io"].is<int>()) return false;
            int gpio = item["io"].as<int>();
            if (!IOConfig::available(gpio) || seen[gpio]) return false;
            seen[gpio] = true;
            const char* oldMode = item["mode"] | "";
            const char* mode = !strcmp(oldMode, "OUT") ? "OUTPUT" : !strcmp(oldMode, "IN") ? "INPUT" :
                               !strcmp(oldMode, "INPULL") ? "INPUT_PULLUP" : !strcmp(oldMode, "INPULLD") ? "INPUT_PULLDOWN" : oldMode;
            JsonObject entry;
            for (JsonObject pin : pins) if (pin["gpio"].as<int>() == gpio) { entry = pin; break; }
            if (entry.isNull()) entry = pins.add<JsonObject>();
            entry["gpio"] = gpio; entry["mode"] = mode;
            entry["label"] = item["lbl"] | "";
            entry["type"] = (item["isA"] | false) ? "ANALOG_INPUT" : "DIGITAL";
            if (!item["inverted"].isNull()) entry["inverted"] = item["inverted"];
            if (!item["initial_state"].isNull()) entry["initial_state"] = item["initial_state"];
        }
        return saveAndApplyIOConfiguration(merged, nullptr, IOEvents::Source::MQTT);
    }

    void loadAndApplyIOConfig() {
        if (!Esp32::spiffsMounted) {
            Serial.println(F("Esp32: SPIFFS not mounted, cannot load I/O config."));
            return;
        }
        if (SPIFFS.exists(Esp32::CONFIG_IO_FILENAME) || SPIFFS.exists(Esp32::CONFIG_IO_FILENAME + ".bak")) {
            Serial.printf("Esp32: Loading I/O config from %s\n", Esp32::CONFIG_IO_FILENAME.c_str());
            String file_content = Storage::readConfigFile(Esp32::CONFIG_IO_FILENAME);
            if (file_content.length() > 0) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, file_content);

        Serial.print(F("Esp32: deserializeJson error code: "));
        Serial.println(error.c_str()); // Print the error code regardless

        if (doc.isNull()) {
            Serial.println(F("Esp32: Parsed JsonDocument is null!"));
        } else {
            Serial.println(F("Esp32: Parsed JsonDocument is NOT null."));
            Serial.print(F("Esp32: !doc[\"io_pins\"].isNull(): "));
            Serial.println(!doc["io_pins"].isNull() ? "true" : "false");
            // The is<JsonArray>() check can remain as is, or be combined:
            if (!doc["io_pins"].isNull()) { // First check if the key exists and is not null
                Serial.print(F("Esp32: doc[\"io_pins\"].is<JsonArray>(): "));
                Serial.println(doc["io_pins"].is<JsonArray>() ? "true" : "false");
            } else {
                Serial.println(F("Esp32: doc[\"io_pins\"] is null, cannot check if it's an array."));
            }
            String serializedDoc;
            serializeJsonPretty(doc, serializedDoc); // Using Pretty for readability
            Serial.println(F("Esp32: Content of parsed 'doc' after deserializeJson:"));
            Serial.println(serializedDoc);
        }

                if (!error) { // If no error from deserializeJson
            Serial.println(F("Esp32: deserializeJson successful. Preparing to call applyIOConfiguration.")); // Added for clarity

            String jsonStringForApply;
            serializeJson(doc, jsonStringForApply); // Serialize the good doc to a new string
            Serial.println(F("Esp32: loadAndApplyIOConfig - Serialized 'doc' to jsonStringForApply:"));
            Serial.println(jsonStringForApply);

            Esp32::applyIOConfiguration(jsonStringForApply, IOEvents::Source::BOOT);
                } else {
                    Serial.print(F("Esp32: Failed to parse "));
                    Serial.print(Esp32::CONFIG_IO_FILENAME);
                    Serial.print(F(" - Error: "));
                    Serial.println(error.c_str()); // Print the actual error code string
                    Serial.println(F("Applying no I/O config from file."));
                }
            } else {
                Serial.printf("Esp32: Failed to read %s or file is empty. Applying no I/O config from file.\n", Esp32::CONFIG_IO_FILENAME.c_str());
            }
        } else {
            Serial.printf("Esp32: %s not found. No I/O pins configured by this system initially.\n", Esp32::CONFIG_IO_FILENAME.c_str());
        }
    }

    String getIOStatusJsonString() {
        IOGuard guard;
        JsonDocument status_doc; // ArduinoJson V7 uses dynamic allocation by default
        status_doc["_revision"] = revision();
        status_doc["friendly_name"] = getFriendlyName();
        JsonArray capabilities = status_doc["pins"].to<JsonArray>();
        for (int gpio = 0; gpio < HUZZAH32; ++gpio) {
            if (!IOConfig::available(gpio)) continue;
            JsonObject item = capabilities.add<JsonObject>();
            item["gpio"] = gpio; item["input_only"] = IOConfig::inputOnly(gpio);
            item["adc"] = IOConfig::analog(gpio); item["adc2"] = IOConfig::adc2(gpio);
            JsonArray usages = item["usages"].to<JsonArray>();
            for (const auto& usage : pinUsages) {
                if (usage.gpio != gpio) continue;
                JsonObject owner = usages.add<JsonObject>(); owner["label"] = usage.label; owner["mode"] = usage.mode;
            }
        }
        JsonArray statuses_array = status_doc["statuses"].to<JsonArray>(); // V7 syntax

        if (Esp32::configured_pins.empty()) {
            // Optional: Serial.println(F("Esp32: No I/O pins configured to report status."));
        }

        for (const IO_Pin_Detail& pin_detail : Esp32::configured_pins) {
            JsonObject pin_status = statuses_array.add<JsonObject>(); // V7 syntax
            pin_status["gpio"] = pin_detail.gpio;
            pin_status["mode"] = pin_detail.mode_str;
            pin_status["type"] = pin_detail.type_str;
            pin_status["label"] = pin_detail.label;
            pin_status["inverted"] = pin_detail.inverted;
            JsonVariantConst calibration;
            for (JsonVariantConst configured : ioDocument["io_pins"].as<JsonArrayConst>())
                if (configured["gpio"].as<int>() == pin_detail.gpio) { calibration = configured["calibration"]; pin_status["icon"] = configured["icon"] | ""; pin_status["zone"] = configured["zone"] | ""; break; }
            if (!calibration.isNull()) {
                pin_status["calibration"] = calibration;
                pin_status["calibrated_value"] = nullptr;
                pin_status["unit"] = calibration["unit"] | "";
            }
            if (const char* usage = reservedPin(pin_detail.gpio)) {
                pin_status["value"] = nullptr;
                pin_status["active"] = nullptr;
                pin_status["value_error"] = "PIN_IN_USE";
                pin_status["reason"] = usage;
                continue;
            }
            if (outputPin(&pin_detail)) {
                pin_status["override_remaining_ms"] = ioOverrides[pin_detail.gpio].remaining(millis());
                pin_status["active"] = getPinActive(pin_detail.gpio);
                pin_status["remaining_ms"] = ioTimers[pin_detail.gpio].remaining(millis());
            }
            if (pin_detail.type_str == "ANALOG_INPUT") {
                if (IOConfig::adc2(pin_detail.gpio) && WiFi.getMode() != WIFI_OFF) {
                    pin_status["value"] = nullptr;
                    pin_status["value_error"] = "ADC2_WIFI";
                } else {
                    int raw = analogRead(pin_detail.gpio);
                    pin_status["value"] = raw;
                    if (!calibration.isNull()) pin_status["calibrated_value"] = IOCalibration::convert(raw, calibration);
                }
            } else {
                pin_status["value"] = digitalRead(pin_detail.gpio);
            }
        }

        String output_json;
        serializeJson(status_doc, output_json);
        return output_json;
    }

    namespace GPS { // Assuming GPS namespace is part of Esp32 or globally accessible
        // const int lon = 77; // Defined in Esp32.h
        // const int lat = 55; // Defined in Esp32.h
    }
} // namespace Esp32
