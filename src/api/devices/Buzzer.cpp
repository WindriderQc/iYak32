// Content will be added in subsequent steps.
// For now, just creating the file.
#include "Buzzer.h"
#include <Arduino.h>

namespace BuzzerModule {
    // Static namespace variables will be defined here
    static int pin_ = -1;
    static bool is_beeping_ = false;
    static unsigned long beep_end_time_ = 0;
    static BUZZER_state current_mode = BUZZER_state::eOFF;

    // Function definitions will go here
    void init(int buzzer_pin) {
        pin_ = buzzer_pin;
        if (pin_ != -1) {
            pinMode(pin_, OUTPUT);
            digitalWrite(pin_, LOW); // Ensure buzzer is off initially
            Serial.printf("BuzzerModule: Initialized on pin %d\n", pin_);
        } else {
            Serial.println(F("BuzzerModule: Not configured or pin is invalid."));
        }
    }

    void loop() {
        if (pin_ == -1) return;

        if (is_beeping_ && millis() >= beep_end_time_) {
            stop(); // Stop the timed beep
        }

        // This switch handles externally set modes (like eINTRO for siren)
        // It's separate from the simple timed beep logic above.
        switch(current_mode) {
            case BUZZER_state::eINTRO:
                playSiren();
                current_mode = BUZZER_state::eOFF; // Reset mode after playing
                break;
            case BUZZER_state::eGOAL: // Example for a goal sound
                // Play a short beep, then reset mode
                beep(1500, 200); // 1.5kHz for 200ms
                current_mode = BUZZER_state::eOFF;
                break;
            case BUZZER_state::ePERIOD_BELL: // Example for period end
                // Play two short beeps
                beep(1000, 250); delay(300); beep(1000, 250);
                current_mode = BUZZER_state::eOFF;
                break;
            // eON and eOFF are more for the timed beep logic or continuous on/off
            case BUZZER_state::eON:
                // If needed for continuous sound, ensure 'stop()' is called to end it.
                // For now, this state doesn't do anything by itself in loop.
                break;
            case BUZZER_state::eOFF:
                // Buzzer is naturally off if not beeping.
                break;
            default:
                break;
        }
    }

    void beep(unsigned int frequency, unsigned int duration_ms) {
        if (pin_ == -1) return;
        // For simple non-blocking beep, we use our timed mechanism
        tone(pin_, frequency);
        is_beeping_ = true;
        beep_end_time_ = millis() + duration_ms;
    }

    void beep(unsigned int duration_ms) { // Simple beep with default frequency
        if (pin_ == -1) return;
        tone(pin_, 1000); // Default frequency 1kHz
        is_beeping_ = true;
        beep_end_time_ = millis() + duration_ms;
    }

    void stop() {
        if (pin_ == -1) return;
        noTone(pin_);
        digitalWrite(pin_, LOW);
        is_beeping_ = false;
    }

    void setMode(BUZZER_state new_mode) {
        // If the new mode is a one-shot sound trigger like eINTRO, eGOAL, ePERIOD_BELL
        // ensure any ongoing timed beep is stopped first.
        if (new_mode != BUZZER_state::eON && new_mode != BUZZER_state::eOFF) {
            if(is_beeping_) stop();
        }
        current_mode = new_mode;
        // If setting to eON for continuous sound (not a timed beep)
        if (new_mode == BUZZER_state::eON && pin_ != -1) {
             // Potentially start a default continuous tone or expect another call to specify frequency.
             // For now, this just sets the mode. A specific function like startContinuousTone(freq) might be better.
             // To make eON immediately effective with a default sound:
             // tone(pin_, 1000); // Example: 1kHz continuous
             // is_beeping_ = false; // So loop doesn't stop it
        } else if (new_mode == BUZZER_state::eOFF && pin_ != -1) {
            stop(); // Ensure buzzer is off
        }
    }

    void playSiren() { // This is a blocking function due to delays
        if (pin_ == -1) {
            Serial.println(F("BuzzerModule: Cannot play siren, pin not configured."));
            return;
        }
        // Ensure any timed beep is stopped before starting siren
        if(is_beeping_) stop();

        const int quart = 250;
        const int half = 500;
        // const int quart3 = 750; // Unused
        const int note = 1000;
        const int note2 = 2000;

        tone(pin_, 523, half); delay(half);
        tone(pin_, 784/2, half); delay(half);
        tone(pin_, 440, half); delay(half);
        tone(pin_, 494, half); delay(half);
        tone(pin_, 523, half); delay(half);
        tone(pin_, 784/2, half); delay(half);
        tone(pin_, 440, half); delay(half);
        tone(pin_, 494, half); delay(half);
        tone(pin_, 555, half); delay(half);
        tone(pin_, 408, half); delay(half);
        tone(pin_, 462, half); delay(half);
        tone(pin_, 523, half); delay(half);
        tone(pin_, 555, half); delay(half);
        tone(pin_, 408, half); delay(half);
        tone(pin_, 462, half); delay(half);
        tone(pin_, 523, half); delay(half);
        tone(pin_, 587, half); delay(half);
        tone(pin_, 440, half); delay(400);
        tone(pin_, 494, half); delay(400);
        tone(pin_, 555, half); delay(400);
        tone(pin_, 587, half); delay(400);
        tone(pin_, 440, half); delay(400);
        tone(pin_, 494, half); delay(400);
        tone(pin_, 555, half); delay(400);
        tone(pin_, 587, note2); delay(note);
        tone(pin_, 440, 300); delay(350);
        tone(pin_, 587, 350); delay(350);
        tone(pin_, 738, quart); delay(half);
        tone(pin_, 440*2, half); delay(note);
        tone(pin_, 738, quart); delay(half);
        tone(pin_, 440*2, note); delay(2000);
        tone(pin_, 587, 1500); delay(half);
        noTone(pin_); // Ensure siren stops
    }
} // namespace BuzzerModule
