#pragma once

#include <Arduino.h>
#include <vector>
#include <SPIFFS.h> // Added for SPIFFS access
#include "api/devices/Buzzer.h" // For BuzzerModule (even if not used yet, good for future)

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
    
    // HOCKEY_state state = HOCKEY_state::eINTRO; // Moved to class member
    // unsigned int tictac = 0;                   // Moved to class member


    // Sensors and Static ISR function for the interrupts

    Sensor::AnLux senseLeft;
    Sensor::AnLux senseRight;

   


    class Hockey
    {
    public:
        Hockey() : previous_state_(HOCKEY_state::eGAMEOVER),
                   current_game_state_(HOCKEY_state::eINTRO),
                   tictac_(0),
                   lastLoop(0),
                   pause_button_last_state_(HIGH) {} // Initialize pause_button_last_state_

        ~Hockey() {}

        void setup()  
        { 
            //Esp32::configPin(LEFTGOAL, "IN", "LEFTGOAL");   //  TODO  sera a changer avec le hardware
           // Esp32::configPin(RIGHTGOAL, "IN", "RIGHTGOAL");
            Esp32::configPin(RESET, "INPULL", "RESET");
            Esp32::configPin(PAUSE, "INPULL", "PAUSE");
            Esp32::configPin(REDLED, "OUT", "REDLED");
            Esp32::ioSwitch(REDLED);

        

            updateSevenSegmentDisplay(scoreString.c_str());

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
            if (current_game_state_ != previous_state_) { // Use current_game_state_
                Serial.print(F("Hockey Game State: "));
                switch (current_game_state_) { // Use current_game_state_
                    case HOCKEY_state::eINTRO:
                        Serial.println(F("Intro"));
                        break;
                    case HOCKEY_state::eON:
                        Serial.println(F("Game ON"));
                        break;
                    case HOCKEY_state::eGOALLEFT:
                        Serial.println(F("Goal Left!"));
                        // Note: goalLeft() itself also prints "GOAL L ". This new log is for state entry.
                        break;
                    case HOCKEY_state::eGOALRIGHT:
                        Serial.println(F("Goal Right!"));
                        // Note: goalRight() itself also prints "GOAL R ". This new log is for state entry.
                        break;
                    case HOCKEY_state::ePERIOD_BELL:
                        Serial.println(F("Period Bell / Intermission"));
                        break;
                    case HOCKEY_state::eDROP_PUCK:
                        Serial.println(F("Puck Drop Sequence"));
                        break;
                    case HOCKEY_state::eGAMEOVER:
                        Serial.println(F("Game Over"));
                        break;
                    case HOCKEY_state::ePAUSE:
                        Serial.println(F("Game Paused"));
                        break;
                    default:
                        Serial.println(F("Unknown State"));
                        break;
                }
                previous_state_ = current_game_state_; // Use current_game_state_
            }
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

            // Pause button edge detection logic
            bool current_pause_button_state = digitalRead(PAUSE);
            if (current_pause_button_state == LOW && pause_button_last_state_ == HIGH) {
                pause();
            }
            pause_button_last_state_ = current_pause_button_state;

            String msg;
           
            switch(current_game_state_) // Use current_game_state_
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
                      
                         
                        updateSevenSegmentDisplay("COOL");
                        tictac_++; // Use tictac_
                        if(tictac_ >= GOAL_DELAY) { tictac_ = 0;   current_game_state_ = HOCKEY_state::eDROP_PUCK; } // Use tictac_ and current_game_state_
                        break;

                case HOCKEY_state::eON:
                     
                        // Calculate elapsed time in seconds
                        elapsedSeconds = time / 1000;

                        // Calculate minutes and seconds
                        minutes = elapsedSeconds / 60;
                        seconds = elapsedSeconds % 60;

                        // Update time display
                        timeString = String("") + doubleDigit(minutes) + String(":") + doubleDigit(seconds);
                        updateSevenSegmentDisplay(timeString.c_str());
                        
                        // Update time
                        time -= delta;
                        
                        // Pause the timer if it reaches 0
                        if(time <= 0) {
                            time = 0;
                            period++;
                            current_game_state_ = HOCKEY_state::ePERIOD_BELL; // Use current_game_state_
                        }
                        
                        msg = senseLeft.loop();
                        if(msg.length() != 0) { goalLeft(); Serial.println(msg);  }
                        
                        msg = senseRight.loop(); 
                        if(msg.length() != 0) {  goalRight();  Serial.println(msg);   }
                        break;

                case HOCKEY_state::eGOALLEFT:
                case HOCKEY_state::eGOALRIGHT:              
                        
                        updateSevenSegmentDisplay("GOAL");
                        tictac_++; // Use tictac_
                        if(tictac_ >= GOAL_DELAY*0.6) { tictac_ = 0; current_game_state_ = HOCKEY_state::eDROP_PUCK; } // Use tictac_ and current_game_state_
                        break;

                case HOCKEY_state::eGAMEOVER:

                        tictac_++; // Use tictac_
                        if(tictac_ >= GOAL_DELAY) {
                            tictac_ = 0;
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  updateSevenSegmentDisplay("Good");
                        else         updateSevenSegmentDisplay("GAME");
                    break;

                case HOCKEY_state::ePERIOD_BELL:
                     
                        if(period < 4) {   
                            tictac_++; // Use tictac_
                            if(tictac_ >= GOAL_DELAY*2) {
                                tictac_ = 0;
                                current_game_state_ = HOCKEY_state::eDROP_PUCK; // Use current_game_state_
                                time = periodLength;
                                Serial.println(F("PERIOD FINISH - Next puck drop..."));
                            }
                            if(tictac_ >= GOAL_DELAY) updateSevenSegmentDisplay(scoreString.c_str());
                            else        updateSevenSegmentDisplay("COOL");
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

                                current_game_state_ = HOCKEY_state::eGAMEOVER; // Use current_game_state_
                                period = 3;
                                tictac_ = 0; // Use tictac_
                        }
                        break;

                case HOCKEY_state::eDROP_PUCK:
                        updateSevenSegmentDisplay(" GO ");
                        //asciiDisplay.displayString(scoreString.c_str());
                        tictac_++; // Use tictac_
                        if(tictac_ >= GOAL_DELAY) {
                            tictac_ = 0;
                            current_game_state_ = HOCKEY_state::eON; // Use current_game_state_
                            Serial.println(F("GO!"));
                            }
                        break;

                case HOCKEY_state::ePAUSE:
                        tictac_++; // Use tictac_
                        if(tictac_ >= GOAL_DELAY) {
                            tictac_ = 0;
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  updateSevenSegmentDisplay(scoreString.c_str());
                        else         updateSevenSegmentDisplay("COOL");
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
            tictac_ = 0; // Use tictac_
            time = periodLength;
            Serial.println(F("Game RESET."));
            current_game_state_ = HOCKEY_state::eINTRO; // Use current_game_state_
        }

        void pause() 
        { 
            if( current_game_state_ == HOCKEY_state::ePAUSE)  { // Use current_game_state_
                Serial.println(F("Timer RESUMED."));
                current_game_state_ = HOCKEY_state::eDROP_PUCK; // Use current_game_state_
            } else {
                Serial.println(F("Timer PAUSED."));
                current_game_state_ = HOCKEY_state::ePAUSE; // Use current_game_state_
            }
        }
        
        void goalLeft() 
        { 
            scoreLeft++;
            Serial.println(F("Left Goal Scored!"));
            BuzzerModule::setMode(BuzzerModule::eGOAL);
            current_game_state_ = HOCKEY_state::eGOALLEFT; // Use current_game_state_
        }

        void goalRight() 
        { 
            scoreRight++;
            Serial.println(F("Right Goal Scored!"));
            BuzzerModule::setMode(BuzzerModule::eGOAL);
            current_game_state_ = HOCKEY_state::eGOALRIGHT; // Use current_game_state_
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
        HOCKEY_state getCurrentGameState() const { return current_game_state_; }

        int getLeftDelta() const { return senseLeft.getFluctDetection(); }
        int getRightDelta() const { return senseRight.getFluctDetection(); }
        
    private:
        int scoreLeft = 0;
        int scoreRight = 0;
        int period = 1;
        unsigned long lastLoop;
        int periodLength = 60000;
        int time = 60000;
        const int GOAL_DELAY = 3200; 
        bool bSwitch = false;
    
        String scoreString = "00:00";
        String timeString = "00:00";

        char* goalLeftStatus;
        HOCKEY_state previous_state_;
        String last_displayed_string_on_7segment_;
        HOCKEY_state current_game_state_;
        unsigned int tictac_;
        bool pause_button_last_state_; // Added

    void updateSevenSegmentDisplay(const char* str_to_display) {
        if (last_displayed_string_on_7segment_ != str_to_display) {
            asciiDisplay.displayString(str_to_display);
            last_displayed_string_on_7segment_ = str_to_display;
        }
    }
    };

}

//  because of pragma_once, this will only be delared once if include in multiple h files 
// (should be declare extern to declare in different cpp)
Hockey::Hockey hockey = Hockey::Hockey();  //  single global instance

