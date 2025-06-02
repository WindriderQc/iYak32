#pragma once

#include <Arduino.h>
#include "api/devices/Buzzer.h"
#include "api/devices/sensors/Pushbtn.h" // For Sensor::Pushbtn
#include "api/Esp32.h" // For Esp32::DEVICE_NAME

namespace BasicMode {

// Define the GPIO pins based on user input
const int BUTTON1_PIN = 12;
const int BUTTON2_PIN = 13;
const int BUTTON3_PIN = 34;

const int LED1_PIN = 21;
const int LED2_PIN = 4;
const int LED3_PIN = 15;

// Frequencies and durations for buzzer sounds
const unsigned int SOUND1_FREQ = 1000;
const unsigned int SOUND1_DUR = 200;
const unsigned int SOUND2_FREQ = 1500;
const unsigned int SOUND2_DUR = 200;
const unsigned int SOUND3_FREQ = 2000;
const unsigned int SOUND3_DUR = 200;
const unsigned int SHUTDOWN_SOUND_FREQ = 500;
const unsigned int SHUTDOWN_SOUND_DUR = 100;

class BasicModeImpl {
public:
    BasicModeImpl() : led1_state_(false), led2_state_(false), led3_state_(false) {
        // Constructor
    }

    void setup() {
        // The actual pin mode configuration (INPUT_PULLUP, OUTPUT)
        // is expected to be done by Esp32::loadAndApplyIOConfig()
        // based on data/io_config.json.

        // Setup Pushbutton objects
        // ISensor::setup(const char* name, const String& device_name, int pin_id, bool inverted = false, bool is_interrupt = true, int channel = 0, int address = 0);
        // For Pushbtn, is_interrupt is true by default. inverted might depend on wiring, true if LOW means pressed.
        // Since we are using INPUT_PULLUP, a press will result in a LOW signal.
        // The Pushbtn class message logic `ISensor::message(!state_)` handles this inversion for messages.
        // If getState() is used directly, it will return true for LOW if not inverted in setup, or if inverted=true is passed to setup.
        // Let's assume getState() should return true on press (LOW state with INPUT_PULLUP).
        // The Pushbtn class itself inverts the logic for the message it returns.
        // We will rely on the message from loop() or ensure getState() reflects a "pressed" state correctly.
        // The `Sensor::Pushbtn::loopImpl` returns a message when `!state_` is true (i.e. when state_ is LOW for INPUT_PULLUP).

        button1_.setup("Button1", Esp32::DEVICE_NAME, BUTTON1_PIN, false);
        button2_.setup("Button2", Esp32::DEVICE_NAME, BUTTON2_PIN, false);
        button3_.setup("Button3", Esp32::DEVICE_NAME, BUTTON3_PIN, false);

        // Ensure LEDs are off initially. io_config.json should handle initial_state LOW.
        // However, a direct digitalWrite here ensures it if io_config.json was missing or incorrect.
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);
        digitalWrite(LED3_PIN, LOW);
        led1_state_ = false;
        led2_state_ = false;
        led3_state_ = false;

        Serial.println("BasicMode: Setup complete.");
    }

    void loop() {
        String btn1_msg = button1_.loop();
        String btn2_msg = button2_.loop();
        String btn3_msg = button3_.loop();

        if (!btn1_msg.isEmpty()) {
            Serial.println("BasicMode: Button 1 pressed");
            BuzzerModule::beep(SOUND1_FREQ, SOUND1_DUR);
            led1_state_ = !led1_state_;
            digitalWrite(LED1_PIN, led1_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 1 is now %s\n", led1_state_ ? "ON" : "OFF");
            if (!led1_state_) { // Just turned OFF
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 1 shutdown sound played.");
            }
        }

        if (!btn2_msg.isEmpty()) {
            Serial.println("BasicMode: Button 2 pressed");
            BuzzerModule::beep(SOUND2_FREQ, SOUND2_DUR);
            led2_state_ = !led2_state_;
            digitalWrite(LED2_PIN, led2_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 2 is now %s\n", led2_state_ ? "ON" : "OFF");
            if (!led2_state_) { // Just turned OFF
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 2 shutdown sound played.");
            }
        }

        if (!btn3_msg.isEmpty()) {
            Serial.println("BasicMode: Button 3 pressed");
            BuzzerModule::beep(SOUND3_FREQ, SOUND3_DUR);
            led3_state_ = !led3_state_;
            digitalWrite(LED3_PIN, led3_state_ ? HIGH : LOW);
            Serial.printf("BasicMode: LED 3 is now %s\n", led3_state_ ? "ON" : "OFF");
            if (!led3_state_) { // Just turned OFF
                BuzzerModule::beep(SHUTDOWN_SOUND_FREQ, SHUTDOWN_SOUND_DUR);
                Serial.println("BasicMode: LED 3 shutdown sound played.");
            }
        }
    }

private:
    Sensor::Pushbtn button1_;
    Sensor::Pushbtn button2_;
    Sensor::Pushbtn button3_;

    bool led1_state_;
    bool led2_state_;
    bool led3_state_;
};

// Global instance of BasicModeImpl
BasicModeImpl basicMode;

} // namespace BasicMode
