#pragma once

#include <Arduino.h> // For String, byte, etc.
#include <ArduinoJson.h> // For JsonDocument (declaration needed for extern)
#include "SystemState.h"  // Added for SYS_state enum visibility
#include <vector>       // For std::vector

// Full header includes for classes used in Esp32 namespace extern declarations
#include "WifiManager.h"    // Provides definition for WifiManager
// #include "devices/Buzzer.h" // Removed, Esp32.cpp will include it directly
#include "Hourglass.h"      // Provides definition for Hourglass
#include "IPin.h"           // Provides definition for Pin (which is in IPin.h)
// Note: Storage.h and Mqtt.h might also be needed if Esp32.h itself declares/uses them directly,
// but the immediate issue is for the types of the extern objects.
// Esp32.cpp includes them for its own implementation needs.

namespace Esp32 {
    // Constants and Defines (remain in .h)
    const String DEVICE_NAME = String("ESP_") + String((uint16_t)(ESP.getEfuseMac()>>32));
    const String CONFIG_FILENAME =  "/esp32config.json";
    const String CONFIG_IO_FILENAME = "/io_config.json"; // Added for I/O config
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
    extern byte vBATSampleSize; // Added extern declaration
    extern int battery_monitor_pin_;
    extern Esp32::Pin* ios[HUZZAH32]; // Size must match definition, explicitly namespaced
    extern WifiManager wifiManager;
    // extern Buzzer buzzer; // Removed, buzzer is now a module (BuzzerModule)
    extern Hourglass hourglass;
    extern bool buzzer_enabled_;
    extern int configured_buzzer_pin_;
    extern int mqtt_data_interval_seconds_;
    extern int state_test_variable; // Added for linker diagnostics

    // Generic I/O Configuration
    struct IO_Pin_Detail {
        int gpio;
        String label;
        String mode_str;      // e.g., "OUTPUT", "INPUT", "INPUT_PULLUP"
        byte actual_mode;     // e.g., OUTPUT, INPUT, INPUT_PULLUP (Arduino constants)
        String type_str;      // e.g., "DIGITAL", "ANALOG_INPUT"
        String initial_state_str; // e.g., "LOW", "HIGH", or empty
        bool graph;

        // Constructor for easier initialization
        IO_Pin_Detail(int g = -1, String l = "", String ms = "", byte am = 0, String ts = "", String iss = "", bool gr = false) :
            gpio(g), label(l), mode_str(ms), actual_mode(am), type_str(ts), initial_state_str(iss), graph(gr) {}
    };
    extern std::vector<IO_Pin_Detail> configured_pins;

    // I/O Configuration Core Logic
    void applyIOConfiguration(const JsonDocument& doc); // Pass JsonDocument by const reference
    bool saveAndApplyIOConfiguration(const JsonDocument& doc); // Pass JsonDocument by const reference
    void loadAndApplyIOConfig();
    String getIOStatusJsonString(); // Added declaration

    // Function Declarations
    void setVerboseLog();
    float getCPUTemp();
    int getCPUFreq();
    int getRemainingHeap();
    bool validIO(int io);
    bool validIO(String ioStr);
    void ioSwitch(int pin);
    void ioBlink(int pin, int timeon, int timeoff, int iteration);
    bool togglePin(int gpio); // Added declaration
    Esp32::Pin* configPin(int gpio, const char* pinModeStr, const char* label = "", bool isAnalog = false); // Changed return type
    void reboot();
    int i2cScanner();
    float getBatteryVoltage();
    float getBattRemaining(bool print = false);
    // String getJsonString(JsonDocument& doc, bool isPretty = false); // Moved to JsonTools.h
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
