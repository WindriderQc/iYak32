#pragma once

#include <Arduino.h>

namespace Esp32
{
   
    class IPin
    {    
        public:   
            IPin(byte pinmode, int io, String pinlabel)
            {
                label = pinlabel ;
                mode = pinmode;
                pinID = io;
                pinMode(pinID, mode);
            }

            virtual int read() = 0;
      
            int pinID;
            byte mode;
            String label;   
    };

    class DigitalInput : public Esp32::IPin
    {    
        public:   
            DigitalInput(int io, String label) : IPin(INPUT, io, label) 
            { }

            virtual int read() override 
            {    
                return digitalRead(pinID);
            }
 
    };

    class PullupInput : public Esp32::IPin
    {    
        public:   
            PullupInput(int io, String label) : IPin(INPUT_PULLUP, io, label) 
            { }

            virtual int read() override 
            {    
                return digitalRead(pinID);
            }
 
    };



    class DigitalOutput : public Esp32::IPin
    {    
        public:   
            DigitalOutput(int io, String label) : IPin(OUTPUT, io, label) 
            { }

            virtual int read() override 
            {    
                return digitalRead(pinID);
            }

            virtual void write(int state)  
            {    
                digitalWrite(pinID, state);
                //return digitalRead(pinID);
            }
 
    };


    class AnalogInput : public Esp32::IPin
    {    
        public:   
            AnalogInput(int io, String label) : IPin(INPUT_PULLUP, io, label) 
            { }

            virtual int read() override 
            {    
                return analogRead(pinID);
            }
 
    };






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

             
                if(strcmp(pinmode, "INPULL") == 0) config.pinMode = INPUT_PULLUP;
                else if(strcmp(pinmode, "INPULLD") == 0) config.pinMode = INPUT_PULLDOWN;
                else if(strcmp(pinmode, "IN") == 0) config.pinMode = INPUT;
                else if(strcmp(pinmode, "OUT") == 0) config.pinMode = OUTPUT;
                else Serial.println("ERROR!!!!!!!!!!!!!!!!!!  Wrong pinMode");


                Serial.print(F("Setting gpio: ")); Serial.print(gpio); Serial.print(F(" : ")); Serial.print(pinlabel); Serial.print(F("  gpio: ")); Serial.print(config.gpio); Serial.print(F("  pinMode: ")); Serial.println(config.pinMode);
                
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