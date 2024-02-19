#pragma once

#include "ISensor.h"

namespace Sensor {

    class AnLux : public Sensor::ISensor
    {
    public:
        AnLux()
        : isr_(0)
        {}

        ~AnLux()
        {}

        void reset() override {} 


        String loopImpl() override
        {
            String msg = "";
 
            if(isr_)
            {
                isr_= false;
                int value = digitalRead(pin_id());
                msg = message("Motion!");  
                Serial.println(F("Motion Detected! " + value));
            } 
            
            return msg;
        }        
      
        inline void handleInterrupt() override
        {
            isr_ = true; //  set interupt flag to true
        }

    private:
        volatile int isr_;       

    };
}

