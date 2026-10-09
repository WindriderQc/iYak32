#pragma once

#include <Arduino.h> // For uint8_t, etc.
#include <Wire.h>    // For TwoWire

// Selects BMP280 or BME280 from the chip ID at runtime.

namespace BMX280 {
    // Configuration
    // This can remain a define, or become a settable const float in .cpp
    // If it's to be settable, it would need a setter function and a static variable in .cpp
    #define SEALEVELPRESSURE_HPA (1013.25)

    // Function Declarations
    bool init(uint8_t i2c_addr = 0x76);
    void setTempUnitCelcius(bool useCelcius); // True for Celcius, False for Fahrenheit
    bool isSuccessfullyInitialized();
    bool hasHumidity();
    
    void actualizeWeather(bool printResult = false);
    
    float getTemperature();
    float getPressure();
    float getAltitude();
    float getHumidity(); // Returns NAN for BMP280 (no humidity sensor)

} // namespace BMX280