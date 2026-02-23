#include "SystemLog.h"

namespace SystemLog {

    // Single-instance buffers (one copy across all translation units)
    static SensorEntry sensorLog_[SENSOR_LOG_SIZE];
    static int sensorHead_ = 0;
    static int sensorCount_ = 0;

    static ErrorEntry errorLog_[ERROR_LOG_SIZE];
    static int errorHead_ = 0;
    static int errorCount_ = 0;

    void logSensor(float temp, float pressure, float humidity, float battery, int heap, int rssi) {
        SensorEntry& entry = sensorLog_[sensorHead_];
        entry.timestamp = millis();
        entry.temp = temp;
        entry.pressure = pressure;
        entry.humidity = humidity;
        entry.battery = battery;
        entry.heap = heap;
        entry.rssi = rssi;

        sensorHead_ = (sensorHead_ + 1) % SENSOR_LOG_SIZE;
        if (sensorCount_ < SENSOR_LOG_SIZE) sensorCount_++;
    }

    void logError(const char* msg, uint8_t severity) {
        ErrorEntry& entry = errorLog_[errorHead_];
        entry.timestamp = millis();
        entry.severity = severity;
        strncpy(entry.message, msg, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        errorHead_ = (errorHead_ + 1) % ERROR_LOG_SIZE;
        if (errorCount_ < ERROR_LOG_SIZE) errorCount_++;

        // Print to serial
        const char* sevStr = severity == 0 ? "INFO" : (severity == 1 ? "WARN" : "ERROR");
        Serial.printf("[%s] %s\n", sevStr, msg);
    }

    void clearErrors() {
        errorHead_ = 0;
        errorCount_ = 0;
        Serial.println(F("SystemLog: Error log cleared."));
    }

    String getSensorLogJson() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        int start = (sensorCount_ < SENSOR_LOG_SIZE) ? 0 : sensorHead_;
        for (int i = 0; i < sensorCount_; i++) {
            int idx = (start + i) % SENSOR_LOG_SIZE;
            JsonObject obj = arr.add<JsonObject>();
            obj["t"] = sensorLog_[idx].timestamp;
            obj["temp"] = serialized(String(sensorLog_[idx].temp, 1));
            obj["pres"] = serialized(String(sensorLog_[idx].pressure, 1));
            obj["hum"] = serialized(String(sensorLog_[idx].humidity, 1));
            obj["batt"] = serialized(String(sensorLog_[idx].battery, 2));
            obj["heap"] = sensorLog_[idx].heap;
            obj["rssi"] = sensorLog_[idx].rssi;
        }

        String result;
        serializeJson(doc, result);
        return result;
    }

    String getErrorLogJson() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        int start = (errorCount_ < ERROR_LOG_SIZE) ? 0 : errorHead_;
        for (int i = 0; i < errorCount_; i++) {
            int idx = (start + i) % ERROR_LOG_SIZE;
            JsonObject obj = arr.add<JsonObject>();
            obj["t"] = errorLog_[idx].timestamp;
            obj["sev"] = errorLog_[idx].severity;
            obj["msg"] = errorLog_[idx].message;
        }

        String result;
        serializeJson(doc, result);
        return result;
    }

    int getSensorCount() { return sensorCount_; }
    int getErrorCount() { return errorCount_; }

} // namespace SystemLog
