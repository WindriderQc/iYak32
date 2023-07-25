#pragma once
#include <Arduino.h>

namespace Esp32 
{
 
    #define BATTERY_READ_PIN 35      // Pin used to read battery voltag   (Huzzah32 - A13 - pin 35)

    String batteryText;      // String variable to hold text for battery voltage
    
    float vBAT = 0;          // Float variable to hold battery voltage
    
    byte vBATSampleSize = 5; // How many time we sample the battery



    // Check the battery voltage     vBAT = between 0 and 4.2 expressed as volts
    float getBatteryVoltage()
    {
        vBAT = (127.0f / 100.0f) * 3.30f * float(analogRead(BATTERY_READ_PIN)) / 4095.0f; // Calculates the voltage left in the battery
        return vBAT;                                                                       
    }


    //  Convert batt voltage to %
    float getBattRemaining(bool print = false) 
    {
        for (byte i = 0; i < vBATSampleSize; i++) // Average samples together to minimize false readings
        {
            vBAT += ceilf(getBatteryVoltage() * 100) / 100; // Work out battery voltage from DAC and round to 2 decimal places
        }

        vBAT /= vBATSampleSize;

        if(print) {
            batteryText = String(vBAT) + "V";  
            Serial.print("Battery Voltage: "); 
            Serial.println(batteryText);       
        }
        
        return vBAT;
    }


}