#pragma once

#include <Arduino.h>
#include <vector>

#include "api/devices/SevenSegmentAscii.h"

#include "api/devices/sensors/ISensor.h"
#include "api/devices/sensors/Pushbtn.h"


namespace Hockey  
{

     //  Pins definition
    const int LEFTGOAL = 26;  
    const int RIGHTGOAL = 15;   
    const int RESET = 13;     
    const int PAUSE = 12;    
    const int CLK = 32; // TM1637 Module connection pins (Digital Pins)
    const int DIO =  33; // TM1637 Module connection pins (Digital Pins)

    // XX:XX 7-segment display module 
    TM1637Display display(CLK, DIO);
    SevenSegmentAscii asciiDisplay(display, 5);  // Set the brightness level (0-7)


//  initialize mutex for interrupt handling
    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;  

    Sensor::Pushbtn btnBlue;
    // Static ISR function for the push button
    static void IRAM_ATTR btnBlue_isr() {
            portENTER_CRITICAL_ISR(&mutex);
            btnBlue.handleInterrupt();
            portEXIT_CRITICAL_ISR(&mutex);
    }

    Sensor::Pushbtn btnLight;
    // Static ISR function for the push button
    static void IRAM_ATTR btnLight_isr() {
            portENTER_CRITICAL_ISR(&mutex);
            btnLight.handleInterrupt();
            portEXIT_CRITICAL_ISR(&mutex);
    }


    class Hockey
    {
    public:
        Hockey() {}

        ~Hockey() {}

        void setup()  
        { 
            Esp32::configPin(Esp32::buzzer.speakerPin, "OUT", "Buzzer");
            Esp32::configPin(LEFTGOAL, "INPULL", "LEFTGOAL");   //  TODO  sera a changer avec le hardware
            Esp32::configPin(RIGHTGOAL, "IN", "RIGHTGOAL");
            Esp32::configPin(RESET, "INPULL", "RESET");
            Esp32::configPin(PAUSE, "INPULL", "PAUSE");
        

            asciiDisplay.displayString(scoreString.c_str()); 

            btnBlue.action.mode_ = RISING; 
            btnBlue.action.callback_ = btnBlue_isr;   
            btnBlue.setup("btnBlue", Esp32::DEVICE_NAME, LEFTGOAL, true ); 

            btnBlue.action.mode_ = RISING; 
            btnLight. action.callback_ = btnLight_isr;  
            btnLight.setup("btnLight", Esp32::DEVICE_NAME, RIGHTGOAL, true );
        }

        void loop() 
        {
            // if(digitalRead(LEFTGOAL)) { goalLeft(); } 
            //if(digitalRead(RIGHTGOAL)) goalRight();   
          
         
            if(!digitalRead(RESET)) reset();
            if(!digitalRead(PAUSE)) pause();   
            
            String msg = btnBlue.loop();
            if(msg.length() != 0) goalLeft();
            msg = btnLight.loop();
            if(msg.length() != 0) goalRight();
          
            unsigned long milli = millis();    

            if(!onPause) time -= milli;

            if(milli- hockey_timestamp > 1000) {
                hockey_timestamp += milli;                 
            }
       
         
            String newstr = String("") + doubleDigit(scoreLeft) + String(":") + doubleDigit(scoreRight) ;
            if( newstr != scoreString) 
           {
            scoreString = newstr;
            Serial.println(scoreString);
            asciiDisplay.displayString(scoreString.c_str()); 
           } 
          
           
           
        }

        String doubleDigit(int score) 
        {
            String result = "";
            if(score <10)  result = "0" + String(score); else result = String(score);
            return result;
        }

        int getScoreLeft() { return scoreLeft; }
        int getScoreRight() { return scoreRight; }

        void setPeriodLenght(int lenght ) {  iPeriodLength = lenght;      }

        void reset() {
            scoreLeft = 0;
            scoreRight=0;
            time = iPeriodLength;
            Serial.print("RESET ");
        }

        void pause() {
            
            onPause = !onPause;
            Serial.print("PAUSE ");
           
        }
        
        void goalLeft() { 
            scoreLeft++;
            Serial.print("GOAL L ");
        }
          void goalRight() { 
            scoreRight++;
            Serial.print("GOAL R ");
        }


       

        
    private:
        int scoreLeft = 0;
        int scoreRight = 0;
        int period = 1;
        int time = 0;
        int iPeriodLength = 200;

        unsigned long hockey_timestamp = 0;

        bool onPause = false;
        String scoreString = "00:00";

        char* goalLeftStatus;
    };

}


Hockey::Hockey hockey = Hockey::Hockey();

