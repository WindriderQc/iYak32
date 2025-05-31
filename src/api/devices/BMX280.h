#pragma once

#include <Arduino.h> // For uint8_t, etc.
#include <Wire.h>    // For TwoWire

// Keep this macro definition at the top or in a config file if it varies by hardware build
// Define or undefine USE_BMPvsBME in your platformio.ini build_flags or a global config header
// For example, in platformio.ini: build_flags = -D USE_BMPvsBME
// #define USE_BMPvsBME

namespace BMX280 {
    // Configuration
    // This can remain a define, or become a settable const float in .cpp
    // If it's to be settable, it would need a setter function and a static variable in .cpp
    #define SEALEVELPRESSURE_HPA (1013.25)

    // Function Declarations
    bool init(TwoWire* i2c_bus = &Wire, uint8_t i2c_addr = 0x76);
    void setTempUnitCelcius(bool useCelcius); // True for Celcius, False for Fahrenheit
    bool isSuccessfullyInitialized();
    
    void actualizeWeather(bool printResult = false);
    
    float getTemperature();
    float getPressure();
    float getAltitude();
    float getHumidity(); // Returns 0.0f if BMP280 is used (as it doesn't support humidity)

} // namespace BMX280