#pragma once

#include "ISensor.h"


namespace Sensor {

    class AnLux : public Sensor::ISensor
    {
    public:
        AnLux()   {}

        ~AnLux()   {}

        String loopImpl() override
        {
            String msg = "";
 
            int value = analogRead(pin_id());
Serial.println(value);
           
            if(value >= lastValue + fluctDetection) {    //   High = darker
                //msg = message("Increasing: " + String(value));   
                Serial.println("Decreasing: " + String(value)) ; 
                puckDropped = true;
                lastValue = value;         
            }
            else if(value <= lastValue - fluctDetection) {
               // msg = message("Decreasing: " + String(value));  
                Serial.println("Increasing: " + String(value)) ;  
                puckDetect = true;
                lastValue = value;
            }  
            

            if(puckDetect && puckDropped) {
                 msg = message("Goal: " + String(value));   
                 puckDropped = false;
                 puckDetect = false;
            }


            //msg = String(value);
            return msg;
        }      

        void setDetection(int variation) {
            fluctDetection = variation;
        }  

        void warmup()      {
             int value = analogRead(pin_id());
             lastValue = value;
        }

    private:      
        volatile int lastValue;
        int fluctDetection = 1000;
        bool puckDropped = false;
        bool puckDetect = false;
    };
}

