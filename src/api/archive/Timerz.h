#pragma once

#include <Arduino.h>

#include <vector>

//  Allow to easily configure timers using the esp32 internal interrupt.   Up to 4 timerz can be used.  TODO: limitation de ESP ou juste changer MAXT_IMER??


namespace Timerz
{
    #define MAX_TIMER 4

    enum AlarmType
    {
        eON,
        eOFF,        
        eTOGGLE
    };

    struct ActionInfo
    {
        typedef void (*Callback_t)();
        Callback_t callback_;
        AlarmType type_;
        long duration_;
    };

    struct PinInfo
    {
        byte pin_;
        int mode_;
    };

    class Timerz
    {
    public:
        Timerz()
        {}

        ~Timerz()
        {}

        void setup(const String& name, const PinInfo& pin, const ActionInfo& action, int timer_id = 0)  //  timer from 0-3. TODO: faire un [] avec les name, et prendre le [].lenght pour le timerId. et mettre une limite a 4 nom pour max 4 timer
        {
            name_ = name;

            pin_ = pin;
            action_ = action; 

            pinMode(pin.pin_, pin.mode_);
            
            timer_ = timerBegin(timer_id, 80, true);     // timer 0, MWDT clock period = 12.5 ns * TIMGn_Tx_WDT_CLK_PRESCALE -> 12.5 ns * 80 -> 1000 ns = 1 us, countUp  /* 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up */
            timerAttachInterrupt(timer_, action.callback_, true);     // edge (not level) triggered 
            
            timerAlarmWrite(timer_, action_.duration_ * 1000000, action.type_ == eTOGGLE ? true : false);      // 2000000 * 1 us = 2 s,    autoreload true/false
            timerAlarmEnable(timer_);    // at last enable timer alarm

           

        }


        inline void  handleInterrupt(void)  //  TODO : remove serial et fait dans child class avec des action type pour éviter le switch()
        {
            Serial.print("timer yo handled ");

            // Critical Code here
            portENTER_CRITICAL_ISR(&timerMux);
            switch(action_.type_)
            {
                case eON:
                    pinState_= HIGH;
                    Serial.println("On"); 
                    break;
                case eOFF:
                    pinState_= LOW;
                    Serial.println("Off");
                    break;
                case eTOGGLE:
                    pinState_= !pinState_;
                    Serial.println("toggle");
                    break;  
            }
            portEXIT_CRITICAL_ISR(&timerMux);

            digitalWrite(pin_.pin_, pinState_);

           // timerAlarmWrite(timer_, 86400, true); // // change timer ticks, autoreload true     // 86400 sec =  24h    TODO!!!!!!!!!!!!!


        }


        int pin_id() const
        {
            return pin_.pin_;
        }

       

        String name_;

        PinInfo pin_;

        ActionInfo action_;

    private:
        
        hw_timer_t * timer_ = NULL;

        volatile uint8_t pinState_ = 0; 

        portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

        
    };

}