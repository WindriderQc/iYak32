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
//Serial.println(value);
           
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
                // Cooldown check
                if (millis() - last_goal_report_time_ < GOAL_COOLDOWN_MS_) {
                    msg = ""; // Do not generate a goal message during cooldown
                    // Reset detection flags to prepare for a new event after cooldown
                    puckDropped = false;
                    puckDetect = false;
                    // lastValue is not updated here to allow re-triggering if puck still present after cooldown
                    return msg; // Exit, effectively skipping goal reporting during cooldown
                }

                // If not in cooldown, proceed to report goal
                msg = message("Goal: " + String(value));
                last_goal_report_time_ = millis(); // Update time of this reported goal

                puckDropped = false;
                puckDetect = false;
            }


            //msg = String(value);
            return msg;
        }      

        void setDetection(int variation) {
            fluctDetection = variation;
        }  

        int getFluctDetection() const { return fluctDetection; }

        void warmup()      {
             int value = analogRead(pin_id());
             lastValue = value;
             last_goal_report_time_ = 0; // Initialize cooldown timer during warmup/reset
        }

    private:      
        volatile int lastValue;
        int fluctDetection = 1000;
        bool puckDropped = false;
        bool puckDetect = false;
        unsigned long last_goal_report_time_ = 0;
        static const unsigned int GOAL_COOLDOWN_MS_ = 2000; // Cooldown period of 2 seconds
    };
}

