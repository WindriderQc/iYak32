#pragma once

#include <Arduino.h>
#include <vector>

#include "api/devices/SevenSegmentAscii.h"
#include "api/devices/sensors/ISensor.h"
#include "api/devices/sensors/Pushbtn.h"
#include "api/devices/sensors/AnLux.h"


namespace Hockey  
{

     //  Pins definition
    const int LEFTGOAL = 39;  
    const int RIGHTGOAL = 34;   
    const int RESET = 13;     
    const int PAUSE = 12;    
    const int CLK = 32; // TM1637 Module connection pins (Digital Pins)
    const int DIO =  33; // TM1637 Module connection pins (Digital Pins)
    const int REDLED = 21; 

    // XX:XX 7-segment display module 
    TM1637Display display(CLK, DIO);
    SevenSegmentAscii asciiDisplay(display, 5);  // Set the brightness level (0-7)



    enum HOCKEY_state
    {
        eON,
        eINTRO,
        eGOALLEFT,        
        eGOALRIGHT,
        ePERIOD_BELL, 
        eDROP_PUCK, 
        ePAUSE
    };
    
    HOCKEY_state state = HOCKEY_state::eINTRO;
    unsigned int tictac = 0;


    // Sensors and Static ISR function for the interrupts

    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;   //  initialize mutex for interrupt handling

    Sensor::AnLux senseLeft;
    static void IRAM_ATTR senseLeft_isr() {
            portENTER_CRITICAL_ISR(&mutex);
            senseLeft.handleInterrupt();
            portEXIT_CRITICAL_ISR(&mutex);
    }

    Sensor::AnLux senseRight;
    static void IRAM_ATTR senseRight_isr() {
            portENTER_CRITICAL_ISR(&mutex);
            senseRight.handleInterrupt();
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
            //Esp32::configPin(LEFTGOAL, "IN", "LEFTGOAL");   //  TODO  sera a changer avec le hardware
           // Esp32::configPin(RIGHTGOAL, "IN", "RIGHTGOAL");
            Esp32::configPin(RESET, "INPULL", "RESET");
            Esp32::configPin(PAUSE, "INPULL", "PAUSE");
            Esp32::configPin(REDLED, "OUT", "REDLED");
            Esp32::ioSwitch(REDLED);

        

            asciiDisplay.displayString(scoreString.c_str()); 

            senseLeft.action.mode_ = RISING; 
            senseLeft.action.callback_ = senseLeft_isr;  
            senseLeft.setup("senseLeft", Esp32::DEVICE_NAME, LEFTGOAL, true );

            senseRight.action.mode_ = RISING; 
            senseRight.action.callback_ = senseRight_isr;   
            senseRight.setup("senseRight", Esp32::DEVICE_NAME, RIGHTGOAL, true ); 

        }

        void warmup()
        {
            senseLeft.warmup();
            senseRight.warmup();
        }



        void loop() 
        {
            
            unsigned long elapsedSeconds;
            unsigned int minutes, seconds;
            String timeString;

            // Actualize time values
            unsigned long currentTime  = millis();  
            unsigned long delta = currentTime - lastLoop;
            lastLoop = currentTime;


            // if score changed via interrupts, actualize scorestring
            String newstr = String("") + doubleDigit(scoreLeft) + String(":") + doubleDigit(scoreRight) ;
            if( newstr != scoreString) 
            {
                scoreString = newstr;
                Serial.println(scoreString);
                //asciiDisplay.displayString(scoreString.c_str()); 
            } 

            if(!digitalRead(RESET)) reset();
            if(!digitalRead(PAUSE)) pause();

            String msg;
            switch(state) 
            {
                case HOCKEY_state::eINTRO:           
                        asciiDisplay.displayString("COOL");
                        tictac++;
                        if(tictac >= GOAL_DELAY*2) { tictac = 0;   state = HOCKEY_state::eON; }
                        break;

                case HOCKEY_state::eON:
                     
                        // Calculate elapsed time in seconds
                        elapsedSeconds = time / 1000;

                        // Calculate minutes and seconds
                        minutes = elapsedSeconds / 60;
                        seconds = elapsedSeconds % 60;

                        // Update time display
                        timeString = String("") + doubleDigit(minutes) + String(":") + doubleDigit(seconds);
                        //Serial.print(timeString);
                        asciiDisplay.displayString(timeString.c_str());
                        
                        // Update time
                        time -= delta;
                        
                        // Pause the timer if it reaches 0
                        if(time <= 0) {
                            time = 0;
                            period++;
                            state = HOCKEY_state::ePERIOD_BELL;
                            Serial.print("BELL");
                        }
                        
                        msg = senseLeft.loop();
                        if(msg.length() != 0) { goalLeft(); Serial.println(msg);  }
                        
                        msg = senseRight.loop(); 
                        if(msg.length() != 0) {  goalRight();  Serial.println(msg);  }
                        break;

                case HOCKEY_state::eGOALLEFT:
                case HOCKEY_state::eGOALRIGHT:              
                        
                        asciiDisplay.displayString("GOAL");
                        tictac++;
                        if(tictac >= GOAL_DELAY*0.6) { tictac = 0; state = HOCKEY_state::eDROP_PUCK; }
                        break;

                case HOCKEY_state::ePERIOD_BELL:
                     
                        if(period < 4) {   
                            tictac++;
                            if(tictac >= GOAL_DELAY*2) { 
                                tictac = 0; 
                                state = HOCKEY_state::eDROP_PUCK; 
                                time = periodLength;
                                Serial.print("PERIOD FINISH");
                            }
                            if(tictac >= GOAL_DELAY) asciiDisplay.displayString(scoreString.c_str());
                            else        asciiDisplay.displayString("COOL");
                        } else {
                            tictac++;
                            if(tictac >= GOAL_DELAY) { 
                                tictac = 0; 
                                bSwitch = !bSwitch;
                            }
                            if(bSwitch)  asciiDisplay.displayString("Good");
                            else         asciiDisplay.displayString("GAME");
                        }
                        break;

                case HOCKEY_state::eDROP_PUCK:
                        Serial.print("DROPPUCK");
                        asciiDisplay.displayString(scoreString.c_str());
                        tictac++;
                        if(tictac >= GOAL_DELAY*2) { 
                            tictac = 0;   
                            state = HOCKEY_state::eON;
                            Serial.print("GO");
                            }
                        break;

                case HOCKEY_state::ePAUSE:
                        tictac++;
                        if(tictac >= GOAL_DELAY) { 
                            tictac = 0; 
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  asciiDisplay.displayString(scoreString.c_str());
                        else         asciiDisplay.displayString("COOL");
                        break;
            }          
           
        }

        String doubleDigit(int score) 
        {
            // String timeString = String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
            String result = "";
            if(score <10)  result = "0" + String(score); else result = String(score);
            return result;
        }

        int getScoreLeft() { return scoreLeft; }
        int getScoreRight() { return scoreRight; }

        void setPeriodLenght(int lenght ) {  periodLength = lenght;      }

        void reset() 
        {
            scoreLeft = 0;
            scoreRight=0;
            time = periodLength;
            Serial.print("RESET ");
            state = HOCKEY_state::eINTRO;
        }

        void pause() 
        { 
            Serial.print("Timer ");
            if( state == HOCKEY_state::ePAUSE)  {
                Serial.println("Started");
                state = HOCKEY_state::eDROP_PUCK;
            } else {
                Serial.println("Paused");
                state = HOCKEY_state::ePAUSE;
            }
        }
        
        void goalLeft() 
        { 
            scoreLeft++;
            Serial.print("GOAL L ");
            state = HOCKEY_state::eGOALLEFT;
        }

        void goalRight() 
        { 
            scoreRight++;
            Serial.print("GOAL R ");
            state = HOCKEY_state::eGOALRIGHT;
        }


       

        
    private:
        int scoreLeft = 0;
        int scoreRight = 0;
        int period = 1;
        unsigned long lastLoop;
        int periodLength = 60000;
        int time = 60000;
        const int GOAL_DELAY = 100; 
        bool bSwitch = false;
    
        String scoreString = "00:00";

        char* goalLeftStatus;
    };

}


Hockey::Hockey hockey = Hockey::Hockey();

