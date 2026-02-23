#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace SystemLog {

    // --- Sensor History ---
    struct SensorEntry {
        unsigned long timestamp;  // millis()
        float temp;
        float pressure;
        float humidity;
        float battery;
        int heap;
        int rssi;
    };

    const int SENSOR_LOG_SIZE = 120;  // ~10 min at 5s interval, ~3.4KB

    // --- Error Log ---
    struct ErrorEntry {
        unsigned long timestamp;
        char message[80];
        uint8_t severity;  // 0=info, 1=warn, 2=error
    };

    const int ERROR_LOG_SIZE = 30;  // ~2.6KB

    // Function declarations
    void logSensor(float temp, float pressure, float humidity, float battery, int heap, int rssi);
    void logError(const char* msg, uint8_t severity = 2);
    void clearErrors();
    String getSensorLogJson();
    String getErrorLogJson();
    int getSensorCount();
    int getErrorCount();

} // namespace SystemLog
