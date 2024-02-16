#pragma once

#include <Arduino.h>

#include "time/TimeAlarms.h"


namespace Als
{
   
    enum AlarmType
    {
        eNONE,
        eSINGLE, 
        eREPEAT
    };

    struct ActionInfo
    {
        typedef void (*Callback_t)();
        Callback_t callbackOn_;
        Callback_t callbackOff_;
        AlarmType type_;
        byte pin_;
    };


    class AlarmIO
    {
        public:
            AlarmIO()
            {}

            ~AlarmIO()
            {}

            void setup(const String& name, const ActionInfo& action)
            {
                name_ = name;
                action_ = action; 

                pinMode(action_.pin_, OUTPUT); 

                if(action.type_ == AlarmType::eREPEAT) 
                {
                    Serial.println();
                    Serial.println("\nSetting alarms...");
                    onAlarm  = Alarm.alarmRepeat(6, 0, 0, action_.callbackOn_);
                    offAlarm = Alarm.alarmRepeat(23, 59, 59, action_.callbackOff_);  
                    Serial.print(onAlarm); Serial.print("-Default Sunrise: "); Serial.println("6:00:00");
                    Serial.print(offAlarm); Serial.print("-Default Nightfall: "); Serial.println("23:59:59");
                    Serial.println();
                }         
            }

            inline void  handleAlarmOff(void)  
            {
                pinState_= LOW;
                Serial.println("Off");
                isDay = false; 
                     
                digitalWrite(action_.pin_, pinState_);
                Serial.println(action_.pin_);
            }

            inline void  handleAlarmOn(void)  
            {
                pinState_= HIGH;
                Serial.println("On");
                isDay = true; 
                     
                digitalWrite(action_.pin_, pinState_);
                Serial.println(action_.pin_);
            }

            // Actualize the alarms with the new times
            void setOffAlarm(int h, int m, int s) 
            {
                timeOff = AlarmHMS(h,m,s);
                Alarm.write(offAlarm, timeOff);
                Serial.print("...nightfall set - ");  Serial.print(h);  Serial.print(":"); Serial.print(m); Serial.print(":"); Serial.print(s); Serial.print("  io: ");  Serial.println(action_.pin_);
            }
            void setOnAlarm(int h, int m, int s) 
            {
                timeOn = AlarmHMS(h,m,s);
                Alarm.write(onAlarm, timeOn);
                Serial.print("...sunrise set - ");  Serial.print(h);  Serial.print(":"); Serial.print(m); Serial.print(":"); Serial.print(s); Serial.print("  io: ");  Serial.println(action_.pin_);
            }

            int pin_id() const
            {
                return action_.pin_;
            }

           

            String name_;
            ActionInfo action_; 
            bool isDay;
           
         

    private:         
            time_t timeOn;
            time_t timeOff;
            uint8_t pinState_ = 0; 
             
            AlarmID_t onAlarm;  
            AlarmID_t offAlarm; 
    };


    
    AlarmIO pins[HUZZAH32]; // 1x for each ESP32 pin_id
}