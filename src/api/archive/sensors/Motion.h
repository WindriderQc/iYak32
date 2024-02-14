#pragma once

#include "ISensor.h"

namespace Sensor {

    class Motion : public Sensor::ISensor
    {
    public:
        Motion()
        : isr_(0)
        {}

        ~Motion()
        {}

        void reset() override {} 


        String loopImpl() override
        {
            String msg = "";
 
            if(isr_ > 0)
            {
                isr_= 0;
                msg = message("Motion!");  
                Serial.println(F("Motion Detected!"));
            } 
            
            return msg;
        }        
      
        inline void setInterrupted() override
        {
            ++isr_;
        }

    private:
        volatile int isr_;

        Sensor::ActionInfo action;  
       

    };
}

