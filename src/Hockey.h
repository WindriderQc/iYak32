#pragma once

#include <Arduino.h>
#include <vector>

#include "api/devices/SevenSegmentAscii.h"
#include "api/devices/sensors/ISensor.h"
#include "api/devices/sensors/Pushbtn.h"
#include "api/devices/sensors/AnLux.h"

// TODO  tictac methode vrmt pas bonne...   asciiDisplay.displayString("xxx");   fait vrmt étiré une loop...   ptete pas bon pour les watchdog timer :S
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
    // 14 =  buzzer pin  //  defined in esp32.h

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
        eGAMEOVER,
        ePAUSE
    };
    
    HOCKEY_state state = HOCKEY_state::eINTRO;
    unsigned int tictac = 0;


    // Sensors and Static ISR function for the interrupts

    Sensor::AnLux senseLeft;
    Sensor::AnLux senseRight;

   


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

            senseLeft.setup("senseLeft", Esp32::DEVICE_NAME, LEFTGOAL, true );

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
            

            // Actualize time values
            unsigned long currentTime  = millis();  
            unsigned long delta = currentTime - lastLoop;
            lastLoop = currentTime;

            // if score changed via interrupts, actualize scorestring
            String newstr = String("") + doubleDigit(scoreLeft) + String(":") + doubleDigit(scoreRight) ;
            if( newstr != scoreString) 
            {
                scoreString = newstr;
                //Serial.println(scoreString);
                //asciiDisplay.displayString(scoreString.c_str()); 
            } 

            if(!digitalRead(RESET)) reset();
            if(!digitalRead(PAUSE)) pause();

            String msg;
           
            switch(state) 
            {
                case HOCKEY_state::eINTRO:    
                        time = periodLength;
                         // Calculate elapsed time in seconds
                         elapsedSeconds = time / 1000;

                         // Calculate minutes and seconds
                         minutes = elapsedSeconds / 60;
                         seconds = elapsedSeconds % 60;
 
                        // Update time display
                        timeString = String("") + doubleDigit(minutes) + String(":") + doubleDigit(seconds);
                      
                         
                        asciiDisplay.displayString("COOL");
                        tictac++;
                        if(tictac >= GOAL_DELAY) { tictac = 0;   state = HOCKEY_state::eDROP_PUCK; }
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
                        if(msg.length() != 0) {  goalRight();  Serial.println(msg);   }
                        break;

                case HOCKEY_state::eGOALLEFT:
                case HOCKEY_state::eGOALRIGHT:              
                        
                        asciiDisplay.displayString("GOAL");
                        tictac++;
                        if(tictac >= GOAL_DELAY*0.6) { tictac = 0; state = HOCKEY_state::eDROP_PUCK; }
                        break;

                case HOCKEY_state::eGAMEOVER:

                        tictac++;
                        if(tictac >= GOAL_DELAY) { 
                            tictac = 0; 
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  asciiDisplay.displayString("Good");
                        else         asciiDisplay.displayString("GAME");
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
                                // Game finished, save the score with date/time
                                File file = SPIFFS.open("/game_scores.txt", FILE_APPEND);
                                if (file) {
                                    // Get the current date and time
                                    time_t now;
                                    struct tm timeinfo;
                                    if (!getLocalTime(&timeinfo)) {
                                        Serial.println("Failed to obtain time");
                                    } else {
                                        char timeBuffer[20];
                                        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
                                        file.printf("%d,%d,%s\n", scoreLeft, scoreRight, timeBuffer);
                                        Serial.printf("Saved score: %d,%d,%s\n", scoreLeft, scoreRight, timeBuffer);
                                    }
                                    file.close();
                                } else {
                                    Serial.println("Failed to open game_scores.txt for writing");
                                }

                                state = HOCKEY_state::eGAMEOVER;
                                period = 3;
                                tictac = 0;
                        }
                        break;

                case HOCKEY_state::eDROP_PUCK:
                        Serial.print("DROPPUCK");
                        asciiDisplay.displayString(" GO ");
                        //asciiDisplay.displayString(scoreString.c_str());
                        tictac++;
                        if(tictac >= GOAL_DELAY) { 
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

    

        void reset() 
        {
            scoreLeft = 0;
            scoreRight=0;
            period = 1;
            tictac = 0;
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

        int getScoreLeft() { return scoreLeft; }
        int getScoreRight() { return scoreRight; }


        String getScoreString()         {       return scoreString;         }
        String gettimeString()          {       return timeString;          }

        int getPeriod()                 {       return period;              }
        
        void setPeriod(int p) { period = p; }
        void setScoreLeft(int s) { scoreLeft = s; } 
        void setScoreRight(int s) { scoreRight = s; }


        int getPeriodLength()                 {       return periodLength/60000;              }
        void setPeriodLength(int length) { periodLength = length*60000; }

        
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
        String timeString = "00:00";

        char* goalLeftStatus;
    };

}

//  because of pragma_once, this will only be delared once if include in multiple h files 
// (should be declare extern to declare in different cpp)
Hockey::Hockey hockey = Hockey::Hockey();  //  single global instance

