#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>

#include <DHT.h>
#include "MQ2.h"

#define USE_BMPvsBME

#ifdef USE_BMPvsBME
    #include <Adafruit_BMP280.h>
#else    
    #include <Adafruit_BME280.h>
#endif

  

namespace Weather  
{

    #define SEALEVELPRESSURE_HPA (1013.25)
    //#define SEALEVELPRESSURE_HPA (1014)

    #ifdef USE_BMPvsBME
        Adafruit_BMP280 sensor;
    #else  
        Adafruit_BME280 sensor; 
    #endif

    float temp =0.0f;
    float pressure=0.0f;
    float altitude=0.0f;
 
    float bmpHumidity=0.0f;
    bool isCelcius = true;

    // air quality
    float co2 = 0.0f;
    float smoke = 0.0f;
    float lpg = 0.0f;


 

    MQ2 mq2;
    
    /* DHT22
    * work from -40oC to +80oC
    * Accuracy of +/- 0.5oC for temperature and +/-2% for relative Humidity
    * Powered between 3.3V and 5V
    *  It is also important to have in mind that the its sensing period is in average 2seconds (minimum time between readings).
    */
    //   !   if use the sensor on distances less than 20m, a 10K resistor should be connected between Data and VCC pins
    DHT Dht;
    float airHumidity = 0.0f;
    float tempDht = 0.0f;
    int tempLowAlert = 0;
    int HOT_TEMP = 30;
    int COLD_TEMP = 15;
    


    void ReadDHT() 
    {
        airHumidity = Dht.getHumidity();
        tempDht = Dht.getTemperature();

        if (isnan(airHumidity) || isnan(temp))   {  
           // Serial.println("Failed to read from DHT sensor!"); 
            airHumidity = 0.0f;
            tempDht = 0.0f;
        }
    }


    void initWeather(int dhtPin, int mq2Pin )
    {
        Serial.print(F("Weather setup - start\n"));
        Serial.print(F("BME/BMP 280 setup - SDA:"));
        Serial.print(SDA);
        Serial.print(F(" SCL:"));
        Serial.print(SCL);
        Serial.print(F(" setup"));


#ifdef USE_BMPvsBME
    bool status = sensor.begin(0x76);  
        if (!status) {
            Serial.println("Could not find a valid BMP280 sensor, check wiring!");
#else  
    bool status = sensor.begin();  
        if (!status) {  
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
            Serial.print("SensorID was: 0x"); Serial.println(sensor.sensorID(),16);
            Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
            Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
            Serial.print("        ID of 0x60 represents a BME 280.\n");
            Serial.print("        ID of 0x61 represents a BME 680.\n");
#endif           
        }else{
            Serial.println(" completed.");
            Serial.println();
        }


        Serial.print(F("DHT Setup")); 
        pinMode(dhtPin, INPUT);  //  TODO: décider si ca reste ici car en doublon avec les configure pin du device
        Dht.setup(dhtPin);
        Serial.println(F(" completed."));

        Serial.print(F("MQ2 setup"));
        float ro = mq2.begin(mq2Pin);
        Serial.print(F(" completed."));
        Serial.print("  -  Ro: "); Serial.print(ro); Serial.println(" kohm");
        Serial.println(F("Weather setup - end"));
    }

  
    void printWeather() {
        Serial.print("Temperature = ");
        Serial.print(temp);
        if(isCelcius)
            Serial.println(" *C");
        else
            Serial.println(" *F");

        Serial.print("Pressure = ");
        Serial.print(pressure);
        Serial.println(" hPa");

        Serial.print("Approx. Altitude = ");
        Serial.print(altitude);
        Serial.println(" m");
#ifndef USE_BMPvsBME   
        Serial.print("Humidity = ");
        Serial.print(bmpHumidity);
        Serial.println(" %");
#endif 

        Serial.println("DHT:");
        Serial.print("  Humidity = ");
        Serial.print(airHumidity);
        Serial.println(" %");

        Serial.print("  Temp = ");
        Serial.print(tempDht);
        Serial.println(" *C");

        Serial.println();
    }


    void actualizeWeather(bool printResult = false) {
            if(isCelcius)
                temp = sensor.readTemperature();  // celcius 
            else 
                temp = 1.8f * sensor.readTemperature() + 32.0f;  // Convert temperature to Fahrenheit 
            pressure = sensor.readPressure() / 100.0F;  // hPa
            altitude = sensor.readAltitude(SEALEVELPRESSURE_HPA); // meters
        #ifndef USE_BMPvsBME   
            bmpHumidity = sensor.readHumidity();  //  %
        #endif 

        ReadDHT();


        co2 = mq2.readCO();
        smoke = mq2.readSmoke();
        lpg = mq2.readLPG();

        if(printResult) printWeather();
    }


}