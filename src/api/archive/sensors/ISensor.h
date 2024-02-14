#pragma once

#include <Arduino.h>
#include <vector>
//#include "api/IPin.h"

namespace Sensor
{
    struct ActionInfo
    {
        typedef void (*Callback_t)();
        Callback_t callback_;
        int mode_;
    };

    class ISensor
    {
    public:
        ISensor()
        {}

        ~ISensor()
        {}

        void setup(const String& name, const String& deviceID, int pin, const ActionInfo& action, bool setInternalPullup = false)
        {
            name_ = name;
            device_ = deviceID;
          // io_label_ = pin->label;
            pin_ = pin;
            
            if(setInternalPullup) {     pinMode(pin_id(), INPUT_PULLUP);         }   

            attachInterrupt(digitalPinToInterrupt(pin), action.callback_, action.mode_);

            Serial.print(F("Attaching interrupt to pin: "));
            Serial.println(pin);

            specificSetup();
        }


        String message(String msg) const
        {
            String payload = "{\"device\":\"" + device_ + "\", ";
            payload += "\"io\":\"" + String(pin_) + "\", ";
            payload += "\"action_type\":\"" + name_ + "\", ";
            payload += "\"value\": \"" + msg +"\"}";
            return payload;
        }

        String message(int value) const
        {
            String payload = "{\"device\":\"" + device_ + "\", ";
            payload += "\"io\":\"" + String(pin_) + "\", ";
            payload += "\"action_type\":\"" + name_ + "\", ";
            payload += "\"value\": \"" + String(value) + "\"}";
            return payload;
        }

      
        int pin_id() const
        {
            return pin_;
        }

        String loop()
        {
            String msg = loopImpl();
            return msg;
        }

        virtual void specificSetup() {};

        virtual void setInterrupted() = 0;

        virtual void reset() = 0;
    

    private:
        virtual String loopImpl() = 0;
        String device_;
        String io_label_;
        String name_;
        int pin_;
    };
}