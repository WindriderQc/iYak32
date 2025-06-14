#pragma once

#include <Arduino.h>
#include <vector>


namespace Sensor
{
    
    struct ActionInfo
    {
        typedef void (*Callback_t)();
        Callback_t callback_;
        int mode_ = 0;      //  0 = No interrupt
    };


    class ISensor
    {
    public:
        ISensor() : isr_(false)
        {}

        ~ISensor()
        {}

        // Setup method for initializing the sensor
        void setup(const String& name, const String& deviceID, int pin, bool setInternalPullup = false)
        {
            name_ = name;
            device_ = deviceID;
            pin_ = pin;
            
            if(setInternalPullup) {     pinMode(pin, INPUT_PULLUP); }  
            
            if(action.mode_) {
                attachInterrupt(digitalPinToInterrupt(pin), action.callback_, action.mode_);
                //currentInstance = this; // Set the current instance to this
                Serial.print(F("Attaching interrupt to pin: "));
                Serial.print(pin);
                Serial.print(F("Action mode: "));
                Serial.println(action.mode_);
            }
           
            specificSetup();
        }

        // Method to generate sensor message
        virtual String message(String msg) const
        {
            return "{\"device\":\"" + device_ + "\", \"io\":\"" + String(pin_) + "\", \"name\":\"" + name_ + "\", \"value\": \"" + msg +"\"}";
        }

        // Overloaded method to generate sensor message for integer values
        virtual String message(int value) const 
        {
            return message(String(value));
        }

        // Get the pin ID
        int pin_id() const
        {
            return pin_;
        }    

       
        String loop()
        {
            String msg = loopImpl();
            return msg;
        }
       
    
        virtual void handleInterrupt() {
            isr_ = true;  //  set interupt flag to true
        }

        // Method to be implemented by subclasses for sensor-specific setup
        virtual void specificSetup() {};  
        virtual void reset() {};
 
 
        // Static pointer to the current instance
       // static ISensor* currentInstance;
       ActionInfo action;  
    
    protected:
        // Pure virtual methods to be implemented in child classes
       
        virtual String loopImpl() = 0;
        
        String device_;
        String name_;
        int pin_;
        volatile bool isr_;
        
       

    };

     // Define the static pointer
   // ISensor* ISensor::currentInstance = nullptr;

   

}