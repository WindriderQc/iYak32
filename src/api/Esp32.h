#pragma once

#include <Arduino.h> // For String, byte, etc.
#include <ArduinoJson.h> // For JsonDocument (declaration needed for extern)
// Forward declarations for classes used in extern variables
class WifiManager;
class Buzzer;
class Hourglass;
class Pin; // Assuming Pin is a class name from IPin.h

namespace Esp32 {
    // Constants and Defines (remain in .h)
    const String DEVICE_NAME = String("ESP_") + String((uint16_t)(ESP.getEfuseMac()>>32));
    const String CONFIG_FILENAME =  "/esp32config.json";
    const int CONFIG_FILE_MAX_SIZE = 1024;
    const int ADC_Max = 4095;
    #define HUZZAH32  39    //  39 pins for esp32   (Should match array size in .cpp)
    #define WEMOSIO   20

    // Extern Namespace Variables (declarations)
    extern bool isConfigFromServer;
    extern JsonDocument configJson_; // Note: JsonDocument might be tricky with extern if not handled carefully with its constructor.
                                    // If linker errors occur, this might need to be a pointer or accessed via a getter.
                                    // For V6, default constructor for global/static is okay.
    extern String configString_;
    extern bool spiffsMounted;
    extern String batteryText;
    extern float vBAT;
    extern int battery_monitor_pin_;
    extern Pin* ios[HUZZAH32]; // Size must match definition
    extern WifiManager wifiManager;
    extern Buzzer buzzer;
    extern Hourglass hourglass;
    extern bool buzzer_enabled_;
    extern int configured_buzzer_pin_;
    extern int mqtt_data_interval_seconds_;

    // Function Declarations
    void setVerboseLog();
    float getCPUTemp();
    int getCPUFreq();
    int getRemainingHeap();
    bool validIO(int io);
    bool validIO(String ioStr);
    void ioSwitch(int pin);
    void ioBlink(int pin, int timeon, int timeoff, int iteration);
    void configPin(int gpio, const char* pinModeStr, const char* label = "", bool isAnalog = false);
    void reboot();
    int i2cScanner();
    float getBatteryVoltage();
    float getBattRemaining(bool print = false);
    String getJsonString(JsonDocument& doc, bool isPretty = false); // Pass JsonDocument by reference
    void mqttIncoming(char* topic, byte* message, unsigned int length);
    void executeJsonConfig();
    bool loadConfig(bool doExecuteConfig = false, JsonDocument* returnDoc = nullptr); // returnDoc for optional copy
    bool saveConfig(JsonDocument& config, bool do_reboot = false); // Pass JsonDocument by reference
    void setup();
    void loop();

    namespace GPS { // Nested namespace with constants
        const int lon = 77;
        const int lat = 55;
    }

} // namespace Esp32
