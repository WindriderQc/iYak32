#pragma once

#include <Arduino.h>

namespace Esp32
{
    struct PinConfig
    {
        byte pinMode = OUTPUT;
        unsigned int gpio = 21;
        const char* label = "GPIO21";
        bool isAnalog = 0;
        String preConfig = "none"; 
    };

    

    class Pin
    {    
        public:   
            Pin(const char* pinmode, unsigned int gpio, const char* pinlabel, bool isAnalog = false )
            {
                config.label = pinlabel;
                config.gpio = gpio;
                config.isAnalog = isAnalog;

                String mode = "";
                if(strcmp(pinmode, "INPULL") == 0)   {  config.pinMode = INPUT_PULLUP;    mode = "Input Pull Up" ;            } 
                else if(strcmp(pinmode, "INPULLD") == 0) {config.pinMode = INPUT_PULLDOWN;  mode = "Input Pull Down" ;    }
                else if(strcmp(pinmode, "IN") == 0) {config.pinMode = INPUT; mode = "Input" ;    }
                else if(strcmp(pinmode, "OUT") == 0) {config.pinMode = OUTPUT; mode = "Output" ;    }
                else Serial.println("ERROR!!!!!!!!!!!!!!!!!!  Wrong pinMode");


                Serial.print(F("Setting gpio: ")); Serial.print(gpio); Serial.print(F(" : ")); Serial.print(pinlabel); Serial.print(F("  gpio: ")); Serial.print(config.gpio); Serial.print(F("  pinMode: ")); Serial.println(mode);
                
                pinMode(config.gpio, config.pinMode);
            }

            int read() 
            {
                if(config.isAnalog)  
                    return analogRead(config.gpio);
                else
                    return digitalRead(config.gpio);
            }
      
            PinConfig config;
    };
}