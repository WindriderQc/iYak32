#pragma once

#include <Arduino.h>
#include "api/devices/Buzzer.h"
#include "api/devices/sensors/AnalogButton.h"
#include "api/Esp32.h"
#include <ArduinoJson.h>
#include <AceButton.h>

using namespace ace_button;

namespace BasicMode {

// Pin and Sound Constant definitions
const int BUTTON1_PIN = 12;
const int BUTTON2_PIN = 13;
const int BUTTON3_PIN = 27;
const int ANALOG_BUTTON1_PIN = 34;
const int ANALOG_BUTTON2_PIN = 39;
const int LED1_PIN = 21;
const int LED2_PIN = 4; // Corrected pin
const int LED3_PIN = 15;
const unsigned int SOUND1_FREQ = 1000;
const unsigned int SOUND1_DUR = 200;
const unsigned int SOUND2_FREQ = 1500;
const unsigned int SOUND2_DUR = 200;
const unsigned int SOUND3_FREQ = 2000;
const unsigned int SOUND3_DUR = 200;
const unsigned int SHUTDOWN_SOUND_FREQ = 500;
const unsigned int SHUTDOWN_SOUND_DUR = 100;

class BasicModeImpl; // Forward declaration
extern BasicModeImpl basicMode; // Extern declaration for global instance

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState);

class BasicModeImpl {
public:
    AceButton button1_;
    AceButton button2_;
    AceButton button3_;
    Sensor::AnalogButton analogButton1_;
    Sensor::AnalogButton analogButton2_;

    BasicModeImpl() :
        button1_(BUTTON1_PIN),
        button2_(BUTTON2_PIN),
        button3_(BUTTON3_PIN),
        led1_state_(false), led2_state_(false), led3_state_(false) {
        // Constructor
    }

    void setup() {
        Esp32::initializeModeInput(BUTTON1_PIN, INPUT_PULLUP);
        Esp32::initializeModeInput(BUTTON2_PIN, INPUT_PULLUP);
        Esp32::initializeModeInput(BUTTON3_PIN, INPUT_PULLUP);

        ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
        buttonConfig->setEventHandler(handleEvent);
        buttonConfig->setFeature(ButtonConfig::kFeatureClick);

        // Analog Button 1 Setup
        analogButton1_.setup("AnalogButton1", Esp32::DEVICE_NAME, ANALOG_BUTTON1_PIN, false);

        // Analog Button 2 Setup
        analogButton2_.setup("AnalogButton2", Esp32::DEVICE_NAME, ANALOG_BUTTON2_PIN, false);

        Esp32::initializeModeOutput(LED1_PIN, "Led1");
        Esp32::initializeModeOutput(LED2_PIN, "Led2");
        Esp32::initializeModeOutput(LED3_PIN, "Led3");
        led1_state_ = getLed1State();
        led2_state_ = getLed2State();
        led3_state_ = getLed3State();

        Serial.println("BasicMode: Setup complete with AceButton and LED outputs.");
    }

    void loop() {
        if (Esp32::isModeInput(BUTTON1_PIN)) button1_.check();
        if (Esp32::isModeInput(BUTTON2_PIN)) button2_.check();
        if (Esp32::isModeInput(BUTTON3_PIN)) button3_.check();

        String analog_btn1_msg = Esp32::isModeInput(ANALOG_BUTTON1_PIN) ? analogButton1_.loop() : "";
        String analog_btn2_msg = Esp32::isModeInput(ANALOG_BUTTON2_PIN) ? analogButton2_.loop() : "";

        if (!analog_btn1_msg.isEmpty()) {
            JsonDocument doc;
            deserializeJson(doc, analog_btn1_msg);
            int value = doc["value"];
            Serial.printf("BasicMode: %s event detected. Value: %d, Threshold: %d\n", doc["name"].as<const char*>(), value, analogButton1_.getThreshold());
        }

        if (!analog_btn2_msg.isEmpty()) {
            JsonDocument doc;
            deserializeJson(doc, analog_btn2_msg);
            int value = doc["value"];
            Serial.printf("BasicMode: %s event detected. Value: %d, Threshold: %d\n", doc["name"].as<const char*>(), value, analogButton2_.getThreshold());
        }
    }

    // Getter methods
    bool getLed1State() const { return Esp32::getPinActive(LED1_PIN); }
    bool getLed2State() const { return Esp32::getPinActive(LED2_PIN); }
    bool getLed3State() const { return Esp32::getPinActive(LED3_PIN); }
    int getAnalogValue1() const { return analogButton1_.getValue(); }
    int getAnalogThreshold1() const { return analogButton1_.getThreshold(); }
    int getAnalogValue2() const { return analogButton2_.getValue(); }
    int getAnalogThreshold2() const { return analogButton2_.getThreshold(); }

    bool isSoundEnabled() const {
        return isSoundActive;
    }

    void setSoundEnabled(bool enabled) {
        isSoundActive = enabled;
        Serial.printf("BasicMode: Sound is now %s.\n", isSoundActive ? "enabled" : "disabled");
    }

    void setAnalogThreshold1(int t) {
        analogButton1_.setThreshold(t);
        Serial.printf("BasicMode: Analog threshold 1 set to %d\n", t);
    }
    void setAnalogThreshold2(int t) {
        analogButton2_.setThreshold(t);
        Serial.printf("BasicMode: Analog threshold 2 set to %d\n", t);
    }

    void handleButtonEvent(AceButton* button, uint8_t eventType) {
        int pin = button->getPin();
        if (eventType == AceButton::kEventClicked) {
            if (pin == BUTTON1_PIN) {
                Serial.println("BasicMode: Button 1 event detected by BasicModeImpl");
                if(isSoundActive) BuzzerModule::beep(SOUND1_FREQ, SOUND1_DUR);
                if (!Esp32::togglePin(LED1_PIN, IOEvents::Source::BUTTON)) return;
                led1_state_ = getLed1State();
                Serial.printf("BasicMode: LED 1 is now %s\n", led1_state_ ? "ON" : "OFF");
                if (!led1_state_) {
                    if(isSoundActive) BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                    Serial.println("BasicMode: LED 1 shutdown sound played.");
                }
            } else if (pin == BUTTON2_PIN) {
                Serial.println("BasicMode: Button 2 event detected by BasicModeImpl");
                if(isSoundActive) BuzzerModule::beep(SOUND2_FREQ, SOUND2_DUR);
                if (!Esp32::togglePin(LED2_PIN, IOEvents::Source::BUTTON)) return;
                led2_state_ = getLed2State();
                Serial.printf("BasicMode: LED 2 is now %s\n", led2_state_ ? "ON" : "OFF");
                if (!led2_state_) {
                    if(isSoundActive)   BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                    Serial.println("BasicMode: LED 2 shutdown sound played.");
                }
            } else if (pin == BUTTON3_PIN) {
                Serial.println("BasicMode: Button 3 event detected by BasicModeImpl");
                if(isSoundActive) BuzzerModule::beep(SOUND3_FREQ, SOUND3_DUR);
                if (!Esp32::togglePin(LED3_PIN, IOEvents::Source::BUTTON)) return;
                led3_state_ = getLed3State();
                Serial.printf("BasicMode: LED 3 is now %s\n", led3_state_ ? "ON" : "OFF");
                if (!led3_state_) {
                    if(isSoundActive) BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                    Serial.println("BasicMode: LED 3 shutdown sound played.");
                }
            }
        }
    }

private:
    bool led1_state_;
    bool led2_state_;
    bool led3_state_;

    bool isSoundActive = 0;

 
    // Play sound with frequency and duration
    void playSound(unsigned int frequency, unsigned int duration) {
        if (isSoundEnabled()) {
            BuzzerModule::beep(frequency, duration);
        } else {
            Serial.println("BasicMode: Sound is disabled.");
        }
    }
    void toggleLed(int ledPin, bool& ledState) {
        if (!Esp32::togglePin(ledPin, IOEvents::Source::BUTTON)) return;
        ledState = Esp32::getPinActive(ledPin);
        Serial.printf("BasicMode: LED on pin %d is now %s\n", ledPin, ledState ? "ON" : "OFF");
    }
    void shutdownLed(int ledPin, bool& ledState) {
        ledState = false;
        Esp32::setPinActive(ledPin, false, 0, IOEvents::Source::BUTTON);
        Serial.printf("BasicMode: LED on pin %d shutdown sound played and turned OFF.\n", ledPin);
        playSound(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
    }
};

// Global instance DEFINITION
BasicModeImpl basicMode;

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    basicMode.handleButtonEvent(button, eventType);
}

} // namespace BasicMode
