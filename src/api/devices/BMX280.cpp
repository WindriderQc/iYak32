#include "BMX280.h"
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Arduino.h>
#include <Wire.h>
#include <cmath>

namespace BMX280 {
    enum class SensorType { None, BMP280, BME280 };

    static Adafruit_BMP280 bmpSensor;
    static Adafruit_BME280 bmeSensor;
    static SensorType sensorType = SensorType::None;
    static float currentTemp = 0.0f;
    static float currentPressure = 0.0f;
    static float currentAltitude = 0.0f;
    static float currentHumidity = NAN;
    static bool useCelsius = true;

    static void printWeather() {
        Serial.print(F("BMX280 conditions: Temp: "));
        Serial.print(currentTemp);
        Serial.print(useCelsius ? F(" C, Press: ") : F(" F, Press: "));
        Serial.print(currentPressure);
        Serial.print(F(" hPa, Alt: "));
        Serial.print(currentAltitude);
        Serial.print(F(" m"));
        if (hasHumidity()) {
            Serial.print(F(", Hum: "));
            Serial.print(currentHumidity);
            Serial.print(F(" %"));
        }
        Serial.println();
    }

    bool init(uint8_t address) {
        sensorType = SensorType::None;
        Wire.beginTransmission(address);
        Wire.write(0xD0); // Bosch chip ID register
        if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
            Serial.printf("BMX280: Could not read chip ID at 0x%02X.\n", address);
            return false;
        }
        const uint8_t chipId = Wire.read();
        Serial.printf("BMX280: Chip ID at 0x%02X is 0x%02X.\n", address, chipId);

        if (chipId == 0x60 && bmeSensor.begin(address, &Wire)) {
            sensorType = SensorType::BME280;
            Serial.println(F("BME280 sensor initialized."));
        } else if (chipId == BMP280_CHIPID && bmpSensor.begin(address)) {
            sensorType = SensorType::BMP280;
            bmpSensor.setSampling(Adafruit_BMP280::MODE_NORMAL,
                                  Adafruit_BMP280::SAMPLING_X2,
                                  Adafruit_BMP280::SAMPLING_X16,
                                  Adafruit_BMP280::FILTER_X16,
                                  Adafruit_BMP280::STANDBY_MS_500);
            Serial.println(F("BMP280 sensor initialized (humidity unavailable)."));
        } else {
            Serial.println(F("BMX280: Unsupported chip ID or sensor initialization failed."));
            return false;
        }
        actualizeWeather(true);
        return true;
    }

    void setTempUnitCelcius(bool value) { useCelsius = value; }
    bool isSuccessfullyInitialized() { return sensorType != SensorType::None; }
    bool hasHumidity() { return sensorType == SensorType::BME280; }

    void actualizeWeather(bool printResult) {
        if (!isSuccessfullyInitialized()) return;
        if (sensorType == SensorType::BME280) {
            currentTemp = bmeSensor.readTemperature();
            currentPressure = bmeSensor.readPressure() / 100.0f;
            currentAltitude = bmeSensor.readAltitude(SEALEVELPRESSURE_HPA);
            currentHumidity = bmeSensor.readHumidity();
        } else {
            currentTemp = bmpSensor.readTemperature();
            currentPressure = bmpSensor.readPressure() / 100.0f;
            currentAltitude = bmpSensor.readAltitude(SEALEVELPRESSURE_HPA);
            currentHumidity = NAN;
        }
        if (!useCelsius) currentTemp = currentTemp * 9.0f / 5.0f + 32.0f;
        if (printResult) printWeather();
    }

    float getTemperature() { return currentTemp; }
    float getPressure() { return currentPressure; }
    float getAltitude() { return currentAltitude; }
    float getHumidity() { return currentHumidity; }
} // namespace BMX280
