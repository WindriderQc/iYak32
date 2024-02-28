#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#include <Wire.h>    // TODO Dépends de si on utilise i2C...  ne devrait ptete pas etre aussi générique

#include "IPin.h"
#include "Storage.h" 
#include "WifiManager.h"
#include "Hourglass.h"
#include "devices/Buzzer.h"

// Pin qty per model definitions
#define HUZZAH32  39
#define WEMOSIO   20  // tbd..


//TODO : utiliser file system pour creer un json de config qui stock les info au lieu/en complement de l'EEPROM...  

namespace Esp32
{
    //  ESP 32 configuration and helping methods
    const String DEVICE_NAME = String("ESP_") + String((uint16_t)(ESP.getEfuseMac()>>32));

    const int ADC_Max = 4095;    

    #define BATTERY_READ_PIN 35      // Pin used to read battery voltag   (Huzzah32 - A13 - pin 35)

    String batteryText;      // String variable to hold text for battery voltage
    
    float vBAT = 0;          // Float variable to hold battery voltage
    
    byte vBATSampleSize = 5; // How many time we sample the battery





    
    // IO Configuration  ESP32 HUZZAH32
    // *** Note :
    //           you can only read analog inputs on ADC #1 once WiFi has started *** //
    //           PWM is possible on every GPIO pin
    /*
    //DigitalInput _A0( 26, "A0");  // A0 DAC2 ADC#2 not available when using wifi 
    //DigitalInput _A1( 25, "A1");  // A1 DAC1 ADC#2 not available when using wifi
    AnalogInput  _A2( 34, "GAZ");  // A2      ADC#1   Note it is not an output-capable pin! 
    AnalogInput  _A3( 39, "LIGHT");  // A3      ADC#1   Note it is not an output-capable pin! 
    AnalogInput  _A4( 36, "SOIL1");  // A4      ADC#1   Note it is not an output-capable pin! 
    DigitalInput _A5(  4, "HEAT1");  // A5      ADC#2  TOUCH0 
    DigitalInput _SCK( 5, "FAN1");  // SPI SCK
    DigitalInput _MOSI( 18, "PUMP1");   // MOSI
    DigitalInput _MISO( 19, "PUMP2");  // MISO
    // GPIO 16 - RX
    // GPIO 17 - TX
    DigitalOutput _D21( 21, "BLUE"); 
    // 23		            BMP280	            SDA
    // 22		            BMP280	            SCL
    DigitalInput _A6( 14, "DHT");  // A6 ADC#2
    // 32		                                A7 can also be used to connect a 32 KHz crystal
    PullupInput _A8( 15, "MOVE"); // 15		A8 ADC#2
    // 33		             	                A9
    // 27		            	                A10 ADC#2
    // 12	            	          	        A11 ADC#2 This pin has a pull-down resistor built into it, we recommend using it as an output only, or making sure that the pull-down is not affected during boot
    DigitalOutput _A12( 13, "LED1");  // A12  ADC#2  Builtin LED
    AnalogInput  _A13( 35, "VBAT");   // A13 This is general purpose input #35 and also an analog input, which is a resistor divider connected to the VBAT line   Voltage is divided by 2 so multiply the analogRead by 2
    
    //IPin* ioPins[] = { &_A2, &_A3, &_A4, &_A5, &_SCK, &_MOSI, &_MISO, &_D21, &_A6, &_A8, &_A12, &_A13 };
    //
    // end of IO Configuration  */
    Pin* ios[HUZZAH32];  //  39 pins for esp32   //  TODO: could be #defines NBRPIN selon le model WEMOSIO, EPS, etc.

    WifiManager wifiManager;
    Hourglass hourglass;
    Buzzer buzzer;
    
   

    void setVerboseLog()    { esp_log_level_set("*", ESP_LOG_DEBUG);    }  //  require #include <esp_log.h>
  
    float getCPUTemp()      { return temperatureRead();                 }
    
    int getCPUFreq()        { return ESP.getCpuFreqMHz();               }
    
    int getRemainingHeap()  { return ESP.getFreeHeap();                 }

    bool validIO(int io) 
    {
        Serial.print("valid io: ");
        Serial.println(io);
        if(ios[io] != nullptr) {
            if(ios[io]->config.gpio != 0) {
                 return true;
            }
        }

        return false;
    }
   
    bool validIO(String io) { return validIO(io.toInt());               }
    
    void ioSwitch(int pin)  { digitalWrite(pin, !digitalRead(pin));     }
   
    void ioBlink(int pin, int timeon, int timeoff, int iteration)
    {
        for (int i = 0; i < iteration; i++)
        {
            ioSwitch(pin);
            ///delay(timeon);  
            delay(timeon);  
            ioSwitch(pin);
            ///delay(timeoff);  
            delay(timeoff);
        }
    }

    void configPin(int gpio ,  const char* pinMode,  const char* label = "", bool isAnalog = false)/* Accept  "INPULL", "INPULLD", "IN" and "OUT" as pinMode.   */
    {
       // if(ios[gpio] != nullptr) delete(ios[gpio]); 
        ios[gpio] = new Pin(pinMode,gpio, label, isAnalog);
    }

    void reboot() 
    {  
        Serial.println(F("!!!  Rebooting device..."));
        delay(1500);
        ESP.restart();                           
    }

    int i2cScanner()
    {
        byte count = 0;
        Serial.print (" -- I2C Scanning -- \n");
        Wire.begin();
        for (byte i = 8; i < 120; i++)
        {
        Wire.beginTransmission (i);
        if (Wire.endTransmission () == 0)
            {
            Serial.print ("Found address: ");
            Serial.print (i, DEC);
            Serial.print (" (0x");
            Serial.print (i, HEX);
            Serial.println (")");
            count++;
            } // end of good response
        } // end of for loop
        Serial.println ("Scanning Done.");
        Serial.print ("Found ");    Serial.print (count, DEC);    Serial.println (" device(s).");

        return count;
    } 
    
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
            batteryText = String(vBAT);
            Serial.print("Battery Voltage: "); 
            Serial.print(batteryText);  
            Serial.println("V");  
        }
        
        return vBAT;
    }

    void setup() 
    {
        i2cScanner();
        wifiManager.setup();
    }

    void loop() 
    {
        //timeSinceBoot += millis();     TODO: timeSinceboot devrait etre extern
        wifiManager.loop();   //  reconnect if connection is lost and handle OTA
       //TODO  devrait y avoir un readIO ici, non??
        buzzer.loop();     
    }


} // namespace Esp32




