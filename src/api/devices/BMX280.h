#pragma once

#include <Wire.h>
//#include <Adafruit_Sensor.h>



#define USE_BMPvsBME    //  BMP fais juste temp et presure, BME fait aussi l'humitdité

#ifdef USE_BMPvsBME
    #include <Adafruit_BMP280.h>
#else    
    #include <Adafruit_BME280.h>
#endif

  

namespace BMX280  
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

    
    


    bool init()
    {
        Serial.print("BMX280 setup - start\nSDA:SDL - ");
        Serial.print(SDA); Serial.print(":"); Serial.print(SCL);
      
#ifdef USE_BMPvsBME
        bool status = sensor.begin(0x76);  
        if (!status) {
            Serial.println("Could not find a valid BMP280 sensor, check wiring!\nBMX280 setup - Failed");
            return(false);
        }

#else  
    bool status = sensor.begin();  
        if (!status) {  
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
            Serial.print("SensorID was: 0x"); Serial.println(sensor.sensorID(),16);
            Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
            Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
            Serial.print("        ID of 0x60 represents a BME 280.\n");
            Serial.print("        ID of 0x61 represents a BME 680.\n");
            Serial.println(F("BMX280 setup - end"));
        }
#endif           
        else{
            Serial.println(" - BMX280 setup - Success");
            return(true);
        }

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


        if(printResult) printWeather();
    }


}