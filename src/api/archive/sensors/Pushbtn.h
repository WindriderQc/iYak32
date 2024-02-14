#pragma once

#include "ISensor.h"

#define DEBOUNCETIME 10


namespace Sensor
{
    class Pushbtn : public Sensor::ISensor 
    {
    public:
        Pushbtn()
         : isr_(false)
         , state_(false)
        {}

        ~Pushbtn()
        { }

        String loopImpl() override
        {
           // isr_saved = isr_;
            saveDebounceTimeout = debounceTimeout;
            saveLastState  = state_;
            String message = "";

            if(isr_) //interrupt has triggered
            {
                state_ = digitalRead(pin_id());
                if( (state_ == saveLastState) // pin is still in the same state as when intr triggered
                    && (millis() - saveDebounceTimeout > DEBOUNCETIME )) // and it has been low for at least DEBOUNCETIME, then valid keypress
                { 
                    message = ISensor::message(!state_);  //  Input is logic inverted
                    isr_ = false;

                    // if (state_ == HIGH) Serial.printf("Button is pressed and debounced, current tick=%d\n", int(millis()));
                    // else                  Serial.printf("Button is released and debounced, current tick=%d\n", int(millis()));
                }        
            }
            return message;
        }

        inline void setInterrupted() override
        {
            isr_ = true; 
            debounceTimeout = xTaskGetTickCount(); //version of millis() that works from interrupt
        }

        bool getState()
        {
            return state_;
        }

        void reset()
        {}


    private:
        volatile bool isr_;
       // volatile bool isr_saved;
        bool state_;
        bool saveLastState;
        uint32_t saveDebounceTimeout;
        volatile uint32_t debounceTimeout = 0;
    };
}