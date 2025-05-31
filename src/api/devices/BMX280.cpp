#include "BMX280.h"
#include <Wire.h>
#include <Arduino.h> // For Serial, float, etc.

#ifdef USE_BMPvsBME
    #include <Adafruit_BMP280.h>
#else
    #include <Adafruit_BME280.h>
#endif

namespace BMX280 {
    #ifdef USE_BMPvsBME
        static Adafruit_BMP280 bmp_sensor_instance;
        static Adafruit_BMP280& get_sensor_instance() { return bmp_sensor_instance; }
    #else
        static Adafruit_BME280 bme_sensor_instance;
        static Adafruit_BME280& get_sensor_instance() { return bme_sensor_instance; }
    #endif

    static bool _is_initialized = false;
    static float _current_temp = 0.0f;
    static float _current_pressure = 0.0f;
    static float _current_altitude = 0.0f;
    static float _current_humidity = 0.0f; // Only for BME
    static bool _is_celcius = true;
    static uint8_t _i2c_addr = 0x76; // Default address, can be overridden by init()
    // static TwoWire* _i2c_bus_ptr = &Wire; // Store the bus used

    // Moved from header, made static internal
    static void printWeather_internal() {
        Serial.print(F("BMX280 conditions: "));
        Serial.print(F(" Temp: ")); Serial.print(_current_temp); Serial.print(_is_celcius ? " C, " : " F, ");
        Serial.print(F(" Press: ")); Serial.print(_current_pressure); Serial.print(" hPa, ");
        Serial.print(F(" Alt: ")); Serial.print(_current_altitude); Serial.print(" m");
        #ifndef USE_BMPvsBME
            Serial.print(F(", Hum: ")); Serial.print(_current_humidity); Serial.print(" %");
        #endif
        Serial.println();
    }

    bool init(TwoWire* i2c_bus, uint8_t i2c_addr_param) {
        _i2c_addr = i2c_addr_param; // Store the address
        // _i2c_bus_ptr = i2c_bus; // Store the bus

        // Note: Adafruit_BME280::begin takes (hex addr, Wire *theWire)
        // Adafruit_BMP280::begin takes (hex addr, TwoWire *theWire)
        // Both are compatible with TwoWire*
        if (!get_sensor_instance().begin(_i2c_addr, i2c_bus)) {
            #ifdef USE_BMPvsBME
                Serial.println(F("Could not find a valid BMP280 sensor, check wiring or I2C address!"));
            #else
                Serial.println(F("Could not find a valid BME280 sensor, check wiring or I2C address!"));
            #endif
            _is_initialized = false;
            return false;
        }
        _is_initialized = true;
        #ifdef USE_BMPvsBME
            /* Default settings from datasheet. */
            get_sensor_instance().setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                                            Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                                            Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                                            Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                                            Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
            Serial.println(F("BMP280 sensor initialized."));
        #else
            Serial.println(F("BME280 sensor initialized."));
        #endif
        actualizeWeather(true); // Initial read and print
        return true;
    }

    void setTempUnitCelcius(bool useCelcius) {
        _is_celcius = useCelcius;
    }

    bool isSuccessfullyInitialized() {
        return _is_initialized;
    }

    void actualizeWeather(bool printResult) {
        if (!_is_initialized) {
            // Serial.println(F("BMX280: Not initialized, cannot actualize weather.")); // Optional log
            return;
        }

        _current_temp = get_sensor_instance().readTemperature();
        _current_pressure = get_sensor_instance().readPressure() / 100.0F; // hPa
        _current_altitude = get_sensor_instance().readAltitude(SEALEVELPRESSURE_HPA);

        #ifndef USE_BMPvsBME
            _current_humidity = get_sensor_instance().readHumidity();
        #else
            _current_humidity = 0.0f; // BMP280 does not have humidity
        #endif

        if (!_is_celcius) {
            _current_temp = (_current_temp * 9.0 / 5.0) + 32.0; // Convert to Fahrenheit
        }

        if (printResult) {
            printWeather_internal();
        }
    }

    float getTemperature() { return _current_temp; }
    float getPressure() { return _current_pressure; }
    float getAltitude() { return _current_altitude; }
    float getHumidity() {
        #ifndef USE_BMPvsBME
            return _current_humidity;
        #else
            return 0.0f; // BMP280 does not support humidity
        #endif
    }

} // namespace BMX280
