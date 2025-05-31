#include "Esp32.h"
#include <ArduinoJson.h>
#include "WifiManager.h" // For WifiManager object
#include "devices/Buzzer.h" // For Buzzer object
#include "Hourglass.h"    // For Hourglass object
#include "IPin.h"         // For Pin object
#include "Storage.h"      // For Storage functions
#include "Mqtt.h"         // For Mqtt functions/objects (like Mqtt::mqttClient, Mqtt::setup)
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
    byte vBATSampleSize = 5; // Definition added
    int battery_monitor_pin_ = 35; // Default from previous setup, will be overridden by JSON config if present
    Esp32::Pin* ios[HUZZAH32]; // Explicitly namespaced
    WifiManager wifiManager;
    // Buzzer buzzer; // Removed, Buzzer is now a module (BuzzerModule)
    Hourglass hourglass;
    bool buzzer_enabled_ = false;
    int configured_buzzer_pin_ = -1;
    int mqtt_data_interval_seconds_ = 5;
   

    // Function Definitions
    void setVerboseLog() { esp_log_level_set("*", ESP_LOG_DEBUG); }

    float getCPUTemp() { return temperatureRead(); }

    int getCPUFreq() { return ESP.getCpuFreqMHz(); }

    int getRemainingHeap() { return ESP.getFreeHeap(); }

    bool validIO(int io) {
        Serial.print("valid io: ");
        Serial.println(io);
        if(io >= 0 && io < HUZZAH32 && ios[io] != nullptr) { // Added bounds check
            if(ios[io]->config.gpio != 0) { // Assuming gpio 0 might be invalid or unconfigured
                 return true;
            }
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

    void configPin(int gpio ,  const char* pinModeStr,  const char* label, bool isAnalog) {
       if(gpio >= 0 && gpio < HUZZAH32) { // Added bounds check
           if(ios[gpio] != nullptr) {
               delete ios[gpio];
               ios[gpio] = nullptr;
           }
           ios[gpio] = new Pin(pinModeStr, gpio, label, isAnalog);
       } else {
           Serial.printf("Error: GPIO %d is out of bounds for configuration.\n", gpio);
       }
    }

    void reboot() {
        Serial.println(F("!!!  Rebooting device..."));
        delay(1500);
        ESP.restart();
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
        // TODO: Add check here if battery_monitor_pin_ is a valid ADC pin
        vBAT = (127.0f / 100.0f) * 3.30f * float(analogRead(battery_monitor_pin_)) / ADC_Max;
        return vBAT;
    }

    float getBattRemaining(bool print) {
        float voltage_sum = 0.0f;
        for (byte i = 0; i < vBATSampleSize; i++) {
            voltage_sum += ceilf(getBatteryVoltage() * 100) / 100;
        }
        float average_vBAT = voltage_sum / vBATSampleSize;
        vBAT = average_vBAT;
        if(print) {
            batteryText = String(vBAT);
            Serial.print("Battery Voltage: ");
            Serial.print(batteryText);
            Serial.println("V");
        }
        return vBAT;
    }

    String getJsonString(JsonDocument& doc, bool isPretty) { // Changed to pass JsonDocument by reference
        String str = "";
        if (isPretty) {
            serializeJsonPretty(doc, str);
        } else {
            serializeJson(doc, str);
        }
        return str;
    }

    void mqttIncoming(char* topic, byte* message, unsigned int length) {
        String topicStr = String(topic);
        #ifdef VERBOSE
        Serial.println(topicStr);
        Serial.print("Msg bytes: "); Serial.println(length);
        #endif

        if(topicStr.indexOf(Esp32::DEVICE_NAME) >= 0) {
            if(topicStr.indexOf("/configIOs") >= 0) {
                JsonDocument config; // Use local JsonDocument
                DeserializationError error = deserializeJson(config, message, length);
                if (error) {
                    Serial.print(F("deserializeJson() failed: "));  Serial.println(error.c_str()); return;
                }
                for (JsonObject elem : config.as<JsonArray>()) {
                    unsigned int io = elem["io"];
                    const char* mode = elem["mode"];
                    const char* lbl = elem["lbl"];
                    bool isA = elem["isA"]; // ArduinoJson V6 uses bool directly
                    Esp32::configPin(io, mode, lbl, isA);
                }
                Serial.println(F("IO Config received and completed"));
            } else if(topicStr.indexOf("/io/") >= 0) {
                Mqtt::MqttMsg mqttMsg = Mqtt::sliceMqttMsg(topic, message, length);
                int io = atoi(mqttMsg.msgTokens[0]);
                if(Esp32::validIO(io)) {
                    if(topicStr.indexOf("/on") >= 0) {
                        digitalWrite(io, HIGH); Serial.print(F("Setting ON output ")); Serial.println(io); return;
                    } else if(topicStr.indexOf("/off") >= 0) {
                        digitalWrite(io, LOW);  Serial.print(F("Setting OFF output ")); Serial.println(io); return;
                    }
                } else {
                     Serial.println(F("Invalid IO"));
                }
            } else if (topicStr.indexOf("/reboot") >= 0) {
                Esp32::reboot();
            }
        } else { /* Not my business */ }
    }

    void executeJsonConfig() {
        Serial.println("\nExecuting Config!");

        wifiManager.setSSID(configJson_["ssid"].as<String>()); // Use .as<String>() for clarity
        wifiManager.setPASS(configJson_["pass"].as<String>());

        wifiManager.setup(true, configJson_["ssid"].as<String>(), configJson_["pass"].as<String>());

        long gmt_offset = configJson_["gmtOffset_sec"] | -18000L;
        int dst_offset = configJson_["daylightOffset_sec"] | 3600;
        hourglass.setTimezone(gmt_offset, dst_offset);

        if(Esp32::wifiManager.isConnected()) {
            if(hourglass.setupTimeSync()) hourglass.getDateTimeString(true);
        }

        Esp32::buzzer_enabled_ = configJson_["buzzer_enabled"] | false;
        Esp32::configured_buzzer_pin_ = configJson_["buzzer_pin"] | -1;

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

        String mqtturl = configJson_["mqtturl"].as<String>();

        if(Mqtt::isEnabled) {
            Mqtt::mqttClient.setCallback(mqttIncoming);
            if (!Mqtt::setup(DEVICE_NAME, mqtturl, Esp32::spiffsMounted, Mqtt::port)) Serial.print("Mqtt setup fail\n\n");
            else Serial.print("Mqtt setup completed\n\n");
        }
    }

    bool loadConfig( bool doExecuteConfig, JsonDocument* returnDoc) {
        if (!Esp32::spiffsMounted) { // Check if SPIFFS is mounted
            Serial.println(F("Error: SPIFFS not mounted. Cannot load config."));
            return false;
        }
        String name = Esp32::CONFIG_FILENAME;
        if (!SPIFFS.exists(name)) {
            Serial.print("esp32Config file "); Serial.print(name); Serial.println(" not found; using system defaults.");
            return false;
        } else {
            Serial.print("Loading preferences from file ");
            Serial.println(Esp32::CONFIG_FILENAME);
            String file_content = Storage::readFile(Esp32::CONFIG_FILENAME);
            // int config_file_size = file_content.length(); // Already available in Storage::readFile if needed
            // Serial.println("Config file size: " + String(config_file_size)); // Redundant if Storage::readFile logs it

            // Using a local doc for deserialization then copy if successful
            JsonDocument tempDoc; // Use temporary doc
            DeserializationError error = deserializeJson(tempDoc, file_content);
            if (error) {
                Serial.print(F("deserializeJson() failed: ")); Serial.println(error.c_str());
                return false;
            }

            Storage::dumpFile(Esp32::CONFIG_FILENAME);

            if (returnDoc) {
                *returnDoc = tempDoc; // Copy to provided doc if any
            }

            configJson_ = tempDoc; // Assign to global configJson_
            configString_ = getJsonString(configJson_, true); // Update global configString_

            if(doExecuteConfig) executeJsonConfig();
            return true;
        }
    }

    bool saveConfig(JsonDocument& config, bool do_reboot) { // Changed to pass JsonDocument by reference
        if (!Esp32::spiffsMounted) { // Check if SPIFFS is mounted
            Serial.println(F("Error: SPIFFS not mounted. Cannot save config."));
            return false;
        }
        SPIFFS.exists(Esp32::CONFIG_FILENAME) ? Serial.print("Updating ") : Serial.print("Creating ");
        Serial.println(Esp32::CONFIG_FILENAME);

        configJson_ = config; // Update global
        configString_ = getJsonString(configJson_, true);

        Storage::writeFile(Esp32::CONFIG_FILENAME, configString_);
        Storage::dumpFile(Esp32::CONFIG_FILENAME);

        if(do_reboot) Esp32::reboot();
        else loadConfig(false); // Reload to ensure consistency without rebooting & executing
        return true;
    }

    void setup() {
        for (int i = 0; i < HUZZAH32; ++i) { // Initialize ios array
            ios[i] = nullptr;
        }

        Serial.println("\nEsp32 setup\nStarting internal SPIFFS filesystem");

        const int EEPROM_SIZE_FOR_APP = 64;
        EEPROM.begin(EEPROM_SIZE_FOR_APP);
        Serial.println(F("EEPROM Initialized with size for app."));

        // Esp32::battery_monitor_pin_ = 35; // Already initialized at definition

        if(!SPIFFS.begin(false)) {
            Serial.println("SPIFFS Mount failed\nDid not find filesystem - this can happen on first-run initialisation\n  Formatting...");
            ioBlink(LED_BUILTIN,100, 100, 4);
            if (!SPIFFS.begin(true)) {
                Serial.println("SPIFFS mount failed\nFormatting not possible - check if a SPIFFS partition is present for your board?");
                ioBlink(LED_BUILTIN,100, 100, 8);
                spiffsMounted = false;
            } else {
                Serial.println(F("SPIFFS Formatting complete."));
                spiffsMounted = true;
            }
        } else {
            Serial.println("SPIFFS mounted successfully");
            spiffsMounted = true;
            Storage::listDir("/", 4);

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
                configDoc["buzzer_pin"] = -1;
                configDoc["mqttDataIntervalSec"] = 5;
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
        wifiManager.loop();
        if (Esp32::buzzer_enabled_) {
            BuzzerModule::loop(); // Changed to BuzzerModule
        }
        if(Mqtt::isEnabled) Mqtt::loop();
    }

    namespace GPS { // Assuming GPS namespace is part of Esp32 or globally accessible
        // const int lon = 77; // Defined in Esp32.h
        // const int lat = 55; // Defined in Esp32.h
    }
} // namespace Esp32
