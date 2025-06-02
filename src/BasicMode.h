#pragma once

#include <Arduino.h>
#include "api/devices/Buzzer.h"
#include "api/devices/sensors/Pushbtn.h"
#include "api/Esp32.h"

namespace BasicMode {

// Pin and Sound Constant definitions
const int BUTTON1_PIN = 12;
const int BUTTON2_PIN = 13;
const int BUTTON3_PIN = 34;
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

// ISR Wrapper Function PROTOTYPES
void IRAM_ATTR isr_button1();
void IRAM_ATTR isr_button2();
void IRAM_ATTR isr_button3();

class BasicModeImpl {
public:
    Sensor::Pushbtn button1_; // Public for access from ISRs via basicMode global instance
    Sensor::Pushbtn button2_;
    Sensor::Pushbtn button3_;

    BasicModeImpl() : led1_state_(false), led2_state_(false), led3_state_(false) {
        // Constructor
    }

    void setup() {
        // Button 1 Setup
        button1_.action.callback_ = &isr_button1;
        button1_.action.mode_ = FALLING;
        button1_.setup("Button1", Esp32::DEVICE_NAME, BUTTON1_PIN, false);

        // Button 2 Setup
        button2_.action.callback_ = &isr_button2;
        button2_.action.mode_ = FALLING;
        button2_.setup("Button2", Esp32::DEVICE_NAME, BUTTON2_PIN, false);

        // Button 3 Setup
        button3_.action.callback_ = &isr_button3;
        button3_.action.mode_ = FALLING;
        button3_.setup("Button3", Esp32::DEVICE_NAME, BUTTON3_PIN, false);

        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);
        digitalWrite(LED3_PIN, LOW);
        led1_state_ = false;
        led2_state_ = false;
        led3_state_ = false;

        Serial.println("BasicMode: Setup complete with interrupt configuration.");
    }

    void loop() {
        // Serial.println("BasicModeImpl::loop() executing");

        String btn1_msg = button1_.loop();
        // Serial.print("btn1_msg: '"); Serial.print(btn1_msg); Serial.println("'");

        String btn2_msg = button2_.loop();
        // Serial.print("btn2_msg: '"); Serial.print(btn2_msg); Serial.println("'");

        String btn3_msg = button3_.loop();
        // Serial.print("btn3_msg: '"); Serial.print(btn3_msg); Serial.println("'");

        if (!btn1_msg.isEmpty()) {
            Serial.println("BasicMode: Button 1 event detected by BasicModeImpl");
            BuzzerModule::beep(SOUND1_FREQ, SOUND1_DUR);
            led1_state_ = !led1_state_;
            digitalWrite(LED1_PIN, led1_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 1 is now %s\n", led1_state_ ? "ON" : "OFF");
            if (!led1_state_) {
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 1 shutdown sound played.");
            }
        }

        if (!btn2_msg.isEmpty()) {
            Serial.println("BasicMode: Button 2 event detected by BasicModeImpl");
            BuzzerModule::beep(SOUND2_FREQ, SOUND2_DUR);
            led2_state_ = !led2_state_;
            digitalWrite(LED2_PIN, led2_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 2 is now %s\n", led2_state_ ? "ON" : "OFF");
            if (!led2_state_) {
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 2 shutdown sound played.");
            }
        }

        if (!btn3_msg.isEmpty()) {
            Serial.println("BasicMode: Button 3 event detected by BasicModeImpl");
            BuzzerModule::beep(SOUND3_FREQ, SOUND3_DUR);
            led3_state_ = !led3_state_;
            digitalWrite(LED3_PIN, led3_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 3 is now %s\n", led3_state_ ? "ON" : "OFF");
            if (!led3_state_) {
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 3 shutdown sound played.");
            }
        }
    }

    // Getter methods
    bool getLed1State() const { return led1_state_; }
    bool getLed2State() const { return led2_state_; }
    bool getLed3State() const { return led3_state_; }

private:
    bool led1_state_;
    bool led2_state_;
    bool led3_state_;
};

// Global instance DEFINITION
BasicModeImpl basicMode;

// ISR Wrapper Function DEFINITIONS
void IRAM_ATTR isr_button1() {
    basicMode.button1_.handleInterrupt();
}

void IRAM_ATTR isr_button2() {
    basicMode.button2_.handleInterrupt();
}

void IRAM_ATTR isr_button3() {
    basicMode.button3_.handleInterrupt();
}

} // namespace BasicMode
