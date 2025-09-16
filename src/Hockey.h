#pragma once

#include <Arduino.h>
#include <vector>
#include <SPIFFS.h> 
#include "api/Esp32.h" 
#include "api/devices/Buzzer.h" 

#include "api/devices/SevenSegmentAscii.h"
#include "api/devices/sensors/AnLux.h"
#include <ArduinoJson.h>
#include <AceButton.h>

using namespace ace_button;

namespace Hockey  
{
    // Pins definition
    const int LEFTGOAL = 39;  
    const int RIGHTGOAL = 34;   
    const int RESET_PIN = 13;
    const int PAUSE_PIN = 12;
    const int CLK = 32; // TM1637 Module connection pins (Digital Pins)
    const int DIO =  33; // TM1637 Module connection pins (Digital Pins)
    const int REDLED = 21; 

    extern TM1637Display display;
    extern SevenSegmentAscii asciiDisplay;

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
    
    extern Sensor::AnLux senseLeft;
    extern Sensor::AnLux senseRight;

    class Hockey; // Forward declaration
    extern Hockey hockey; // extern declaration

    void handleHockeyEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
        int pin = button->getPin();
        if (eventType == AceButton::kEventClicked) {
            if (pin == PAUSE_PIN) {
                hockey.pause();
            } else if (pin == RESET_PIN) {
                hockey.reset();
            }
        }
    }

    class Hockey
    {
    public:
        Hockey() :
                   pauseButton_(PAUSE_PIN),
                   resetButton_(RESET_PIN),
                   previous_state_(HOCKEY_state::eGAMEOVER),
                   current_game_state_(HOCKEY_state::eINTRO),
                   elapsedTimeInState_ms_(0), 
                   lastLoop(0), // Used for calculating delta in main loop
                   introDuration_ms_(3000UL),      // 3 seconds
                   goalCelebration_ms_(2000UL),    // 2 seconds
                   puckDrop_ms_(3000UL),           // 3 seconds
                   periodIntermission_ms_(6000UL)  // 6 seconds
        { }

        ~Hockey() {}

        void setup()  
        { 
            pinMode(RESET_PIN, INPUT_PULLUP);
            pinMode(PAUSE_PIN, INPUT_PULLUP);

            ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
            buttonConfig->setEventHandler(handleHockeyEvent);
            buttonConfig->setFeature(ButtonConfig::kFeatureClick);

            Esp32::configPin(REDLED, "OUT", "REDLED");
            Esp32::ioSwitch(REDLED);

            updateSevenSegmentDisplay(scoreString.c_str());

            senseLeft.setup("senseLeft", Esp32::DEVICE_NAME, LEFTGOAL, true );
            senseRight.setup("senseRight", Esp32::DEVICE_NAME, RIGHTGOAL, true ); 
        }

        void loop() 
        {
            pauseButton_.check();
            resetButton_.check();

            if (current_game_state_ != previous_state_) {
                Serial.print(F("Hockey Game State: "));
                switch (current_game_state_) {
                    case HOCKEY_state::eINTRO: Serial.println(F("Intro")); break;
                    case HOCKEY_state::eON: Serial.println(F("Game ON")); break;
                    case HOCKEY_state::eGOALLEFT: Serial.println(F("Goal Left!")); break;
                    case HOCKEY_state::eGOALRIGHT: Serial.println(F("Goal Right!")); break;
                    case HOCKEY_state::ePERIOD_BELL: Serial.println(F("Period Bell / Intermission")); break;
                    case HOCKEY_state::eDROP_PUCK: Serial.println(F("Puck Drop Sequence")); break;
                    case HOCKEY_state::eGAMEOVER: Serial.println(F("Game Over")); break;
                    case HOCKEY_state::ePAUSE: Serial.println(F("Game Paused")); break;
                    default: Serial.println(F("Unknown State")); break;
                }
                previous_state_ = current_game_state_;
            }
            unsigned long elapsedSeconds;
            unsigned int minutes, seconds;
            
            unsigned long currentTime  = millis();  
            unsigned long delta = currentTime - lastLoop;
            lastLoop = currentTime;

            String newstr = String("") + doubleDigit(scoreLeft) + String(":") + doubleDigit(scoreRight) ;
            if( newstr != scoreString) 
            {
                scoreString = newstr;
            } 

            String msg;
           
            switch(current_game_state_)
            {
                case HOCKEY_state::eINTRO:    
                        time = periodLength;
                        elapsedSeconds = time / 1000;
                        minutes = elapsedSeconds / 60;
                        seconds = elapsedSeconds % 60;
                        timeString = String("") + doubleDigit(minutes) + String(":") + doubleDigit(seconds);
                        updateSevenSegmentDisplay("COOL");
                        elapsedTimeInState_ms_ += delta;
                        if(elapsedTimeInState_ms_ >= introDuration_ms_) { elapsedTimeInState_ms_ = 0;   current_game_state_ = HOCKEY_state::eDROP_PUCK; }
                        break;

                case HOCKEY_state::eON:
                        elapsedSeconds = time / 1000;
                        minutes = elapsedSeconds / 60;
                        seconds = elapsedSeconds % 60;
                        timeString = String("") + doubleDigit(minutes) + String(":") + doubleDigit(seconds);
                        updateSevenSegmentDisplay(timeString.c_str());
                        
                        time -= delta;
                        
                        if(time <= 0) {
                            time = 0;
                            period++;
                            current_game_state_ = HOCKEY_state::ePERIOD_BELL;
                        }
                        
                        msg = senseLeft.loop();
                        if(msg.length() != 0) { goalLeft(); Serial.println(msg);  }
                        
                        msg = senseRight.loop(); 
                        if(msg.length() != 0) {  goalRight();  Serial.println(msg);   }
                        break;

                case HOCKEY_state::eGOALLEFT:
                case HOCKEY_state::eGOALRIGHT:              
                        updateSevenSegmentDisplay("GOAL");
                        elapsedTimeInState_ms_ += delta;
                        if(elapsedTimeInState_ms_ >= goalCelebration_ms_) { elapsedTimeInState_ms_ = 0; current_game_state_ = HOCKEY_state::eDROP_PUCK; }
                        break;

                case HOCKEY_state::eGAMEOVER:
                        elapsedTimeInState_ms_ += delta;
                        if(elapsedTimeInState_ms_ >= puckDrop_ms_) {
                            elapsedTimeInState_ms_ = 0;
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  updateSevenSegmentDisplay("Good");
                        else         updateSevenSegmentDisplay("GAME");
                    break;

                case HOCKEY_state::ePERIOD_BELL:
                        if(period < 4) {   
                            elapsedTimeInState_ms_ += delta;
                            if(elapsedTimeInState_ms_ >= periodIntermission_ms_) {
                                elapsedTimeInState_ms_ = 0;
                                current_game_state_ = HOCKEY_state::eDROP_PUCK;
                                time = periodLength;
                                Serial.println(F("PERIOD FINISH - Next puck drop..."));
                            }
                            if(elapsedTimeInState_ms_ >= periodIntermission_ms_ / 2) updateSevenSegmentDisplay(scoreString.c_str());
                            else        updateSevenSegmentDisplay("COOL");
                        } else {
                                File file = SPIFFS.open("/game_scores.txt", FILE_APPEND);
                                if (file) {
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

                                current_game_state_ = HOCKEY_state::eGAMEOVER;
                                period = 3;
                                elapsedTimeInState_ms_ = 0;
                        }
                        break;

                case HOCKEY_state::eDROP_PUCK:
                        updateSevenSegmentDisplay(" GO ");
                        elapsedTimeInState_ms_ += delta;
                        if(elapsedTimeInState_ms_ >= puckDrop_ms_) {
                            elapsedTimeInState_ms_ = 0;
                            current_game_state_ = HOCKEY_state::eON;
                            Serial.println(F("GO!"));
                            }
                        break;

                case HOCKEY_state::ePAUSE:
                        elapsedTimeInState_ms_ += delta;
                        if(elapsedTimeInState_ms_ >= puckDrop_ms_) {
                            elapsedTimeInState_ms_ = 0;
                            bSwitch = !bSwitch;
                        }
                        if(bSwitch)  updateSevenSegmentDisplay(scoreString.c_str());
                        else         updateSevenSegmentDisplay("COOL");
                        break;
            }          
        }

        String doubleDigit(int score) 
        {
            String result = "";
            if(score <10)  result = "0" + String(score); else result = String(score);
            return result;
        }

        void reset() 
        {
            scoreLeft = 0;
            scoreRight=0;
            period = 1;
            elapsedTimeInState_ms_ = 0;
            time = periodLength;
            Serial.println(F("Game RESET."));
            current_game_state_ = HOCKEY_state::eINTRO;
        }

        void pause() 
        { 
            if( current_game_state_ == HOCKEY_state::ePAUSE)  {
                Serial.println(F("Timer RESUMED."));
                current_game_state_ = HOCKEY_state::eDROP_PUCK;
            } else {
                Serial.println(F("Timer PAUSED."));
                current_game_state_ = HOCKEY_state::ePAUSE;
            }
        }
        
        void goalLeft() 
        { 
            scoreLeft++;
            Serial.println(F("Left Goal Scored!"));
            BuzzerModule::setMode(BuzzerModule::eGOAL);
            current_game_state_ = HOCKEY_state::eGOALLEFT;
        }

        void goalRight() 
        { 
            scoreRight++;
            Serial.println(F("Right Goal Scored!"));
            BuzzerModule::setMode(BuzzerModule::eGOAL);
            current_game_state_ = HOCKEY_state::eGOALRIGHT;
        }

        int getScoreLeft() { return scoreLeft; }
        int getScoreRight() { return scoreRight; }

        String getScoreString() { return scoreString; }
        String gettimeString() { return timeString; }
        int getPeriod() { return period; }
        
        void setPeriod(int p) { period = p; }
        void setScoreLeft(int s) { scoreLeft = s; } 
        void setScoreRight(int s) { scoreRight = s; }

        int getPeriodLength() { return periodLength/60000; }
        void setPeriodLength(int length) { periodLength = length*60000; }
        HOCKEY_state getCurrentGameState() const { return current_game_state_; }

        int getLeftDelta() const { return senseLeft.getFluctDetection(); }
        int getRightDelta() const { return senseRight.getFluctDetection(); }

        void setIntroDurationMs(unsigned long ms) { introDuration_ms_ = ms; }
        void setGoalCelebrationMs(unsigned long ms) { goalCelebration_ms_ = ms; }
        void setPuckDropMs(unsigned long ms) { puckDrop_ms_ = ms; }
        void setPeriodIntermissionMs(unsigned long ms) { periodIntermission_ms_ = ms; }

        unsigned long getIntroDurationMs() const { return introDuration_ms_; }
        unsigned long getGoalCelebrationMs() const { return goalCelebration_ms_; }
        unsigned long getPuckDropMs() const { return puckDrop_ms_; }
        unsigned long getPeriodIntermissionMs() const { return periodIntermission_ms_; }

        void applySettings(const JsonObjectConst& hockeyConfig) {
            Serial.println(F("Hockey: Applying settings from JSON..."));
            if (hockeyConfig.isNull()) {
                Serial.println(F("Hockey: hockeyConfig is null, cannot apply settings."));
                return;
            }

            if (hockeyConfig["introDurationMs"].is<unsigned long>()) {
                introDuration_ms_ = hockeyConfig["introDurationMs"].as<unsigned long>();
            }
            if (hockeyConfig["goalCelebrationMs"].is<unsigned long>()) {
                goalCelebration_ms_ = hockeyConfig["goalCelebrationMs"].as<unsigned long>();
            }
            if (hockeyConfig["puckDropMs"].is<unsigned long>()) {
                puckDrop_ms_ = hockeyConfig["puckDropMs"].as<unsigned long>();
            }
            if (hockeyConfig["periodIntermissionMs"].is<unsigned long>()) {
                periodIntermission_ms_ = hockeyConfig["periodIntermissionMs"].as<unsigned long>();
            }
            if (hockeyConfig["periodLengthMinutes"].is<int>()) {
                int pl_minutes = hockeyConfig["periodLengthMinutes"].as<int>();
                if (pl_minutes > 0) {
                    periodLength = pl_minutes * 60000UL;
                }
            }
            if (hockeyConfig["leftDelta"].is<int>()) {
                senseLeft.setDetection(hockeyConfig["leftDelta"].as<int>());
            }
            if (hockeyConfig["rightDelta"].is<int>()) {
                senseRight.setDetection(hockeyConfig["rightDelta"].as<int>());
            }
            Serial.print(F("Hockey: Settings applied. Current period length (ms): ")); Serial.println(periodLength);
        }

        void populateSettings(JsonObject& hockeyConfig) {
            if (hockeyConfig.isNull()) {
                Serial.println(F("Hockey: hockeyConfig is null, cannot populate settings."));
                return;
            }
            hockeyConfig["introDurationMs"] = introDuration_ms_;
            hockeyConfig["goalCelebrationMs"] = goalCelebration_ms_;
            hockeyConfig["puckDropMs"] = puckDrop_ms_;
            hockeyConfig["periodIntermissionMs"] = periodIntermission_ms_;
            hockeyConfig["periodLengthMinutes"] = periodLength / 60000UL;
            hockeyConfig["leftDelta"] = senseLeft.getFluctDetection();
            hockeyConfig["rightDelta"] = senseRight.getFluctDetection();
            Serial.println(F("Hockey: Settings populated into JSON."));
        }
        
    private:
        AceButton pauseButton_;
        AceButton resetButton_;

        int scoreLeft = 0;
        int scoreRight = 0;
        int period = 1;
        unsigned long lastLoop;
        int periodLength = 60000;
        int time = 60000;
        bool bSwitch = false;
    
        String scoreString = "00:00";
        String timeString = "00:00";

        char* goalLeftStatus;
        HOCKEY_state previous_state_;
        String last_displayed_string_on_7segment_;
        HOCKEY_state current_game_state_;
        unsigned long elapsedTimeInState_ms_;

        unsigned long introDuration_ms_;
        unsigned long goalCelebration_ms_;
        unsigned long puckDrop_ms_;
        unsigned long periodIntermission_ms_;

        void updateSevenSegmentDisplay(const char* str_to_display) {
            if (last_displayed_string_on_7segment_ != str_to_display) {
                asciiDisplay.displayString(str_to_display);
                last_displayed_string_on_7segment_ = str_to_display;
            }
        }
    };
}

extern Hockey::Hockey hockey;
