#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#include "Battery.h"
#include "IPin.h"


// Internal filesystem (SPIFFS)
// used for non-volatile camera settings
#include "storage.h"

// Pin qty per model definitions
#define HUZZAH32  39
#define WEMOSIO   20  // tbd..






namespace Esp32
{
    //  ESP 32 configuration and helping methods
    const String DEVICE_NAME = String("ESP_") + String((uint16_t)(ESP.getEfuseMac()>>32));

    
    // IO Configuration
    // ESP32 HUZZAH32
   
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

   



    void setVerboseLog()    { esp_log_level_set("*", ESP_LOG_DEBUG);    }  //  require #include <esp_log.h>
    void ioSwitch(int pin)  { digitalWrite(pin, !digitalRead(pin));     }
    float getCPUTemp()      { return temperatureRead();                 }
    int getCPUFreq()        { return ESP.getCpuFreqMHz();               }
    int getRemainingHeap()  { return ESP.getFreeHeap();                 }
  
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

    /* Take  "INPULL", "INPULLD", "IN" and "OUT" as pinMode. Label  */
    void configPin(int gpio ,  const char* pinMode,  const char* label = "", bool isAnalog = false)
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
    bool validIO(String io)    { return validIO(io.toInt()); }


   
 //   void setupOTA(const char* ssid = "SM-A520W7259", const char* password = "Alouette54321")
  //  void setupOTA(const char* ssid, const char* password)
    /*void setupOTA()
    {
        WiFi.mode(WIFI_STA);
       // WiFi.begin(ssid, password);
        WiFi.begin("UGLink", "Alouette54321!");
        
        uint32_t notConnectedCounter = 0;
        Serial.print("\nWifi connecting...");
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            Serial.print("...");
            notConnectedCounter++;
            if(notConnectedCounter > 150) { // Reset board if not connected after 15s
                Serial.println("\nRebooting due to Wifi not connecting...");
                ESP.restart();
            }
        }

        // Port defaults to 3232
        // ArduinoOTA.setPort(3232);

        // Hostname defaults to esp3232-[MAC]
        // ArduinoOTA.setHostname("myesp32");

        // No authentication by default
        // ArduinoOTA.setPassword("admin");

        // Password can be set with it's md5 value as well
        // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
        // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

        ArduinoOTA
            .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
            })
            .onEnd([]() {
            Serial.println("\nEnd");
            })
            .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            })
            .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });

        ArduinoOTA.begin();

        Serial.println("Ready");
        Serial.print("IP address: ");
        Serial.println(getLocalIP());
    }*/


    void loop() 
    {
        //timeSinceBoot += millis();     TODO: timeSinceboot devrait etre extern
      
       
    }




} // namespace Esp32




