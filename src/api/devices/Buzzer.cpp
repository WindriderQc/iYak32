#include "Buzzer.h"
#include <Arduino.h>

namespace BuzzerModule {
    static int pin_ = -1;
    static bool is_beeping_ = false;
    static unsigned long beep_end_time_ = 0;
    static BUZZER_state current_mode = BUZZER_state::eOFF;

    // Non-blocking sequence playback state
    static const Note* current_sequence_ = nullptr;
    static int sequence_length_ = 0;
    static int sequence_index_ = 0;
    static unsigned long sequence_next_time_ = 0;
    static bool sequence_playing_tone_ = false;

    // Siren melody (matches original playSiren timing)
    static const Note siren_sequence[] = {
        {523, 500, 0}, {392, 500, 0}, {440, 500, 0}, {494, 500, 0},
        {523, 500, 0}, {392, 500, 0}, {440, 500, 0}, {494, 500, 0},
        {555, 500, 0}, {408, 500, 0}, {462, 500, 0}, {523, 500, 0},
        {555, 500, 0}, {408, 500, 0}, {462, 500, 0}, {523, 500, 0},
        {587, 500, 0}, {440, 500, 0}, {494, 500, 0}, {555, 500, 0},
        {587, 400, 0}, {440, 400, 0}, {494, 400, 0}, {555, 400, 0},
        {587, 2000, 0}, {440, 300, 50}, {587, 350, 0},
        {738, 250, 250}, {880, 500, 500}, {738, 250, 250},
        {880, 1000, 1000}, {587, 1500, 0}
    };
    static const int SIREN_LENGTH = sizeof(siren_sequence) / sizeof(siren_sequence[0]);

    // Period bell: two short beeps
    static const Note period_bell_sequence[] = {
        {1000, 250, 300}, {1000, 250, 0}
    };
    static const int PERIOD_BELL_LENGTH = sizeof(period_bell_sequence) / sizeof(period_bell_sequence[0]);

    // Alert: 3 short high beeps for error notifications
    static const Note alert_sequence[] = {
        {2500, 80, 80}, {2500, 80, 80}, {2500, 80, 0}
    };
    static const int ALERT_LENGTH = sizeof(alert_sequence) / sizeof(alert_sequence[0]);

    static void startSequence(const Note* seq, int length) {
        if (pin_ == -1) return;
        if (is_beeping_) stop();
        current_sequence_ = seq;
        sequence_length_ = length;
        sequence_index_ = 0;
        sequence_playing_tone_ = false;
        sequence_next_time_ = millis();
    }

    void init(int buzzer_pin) {
        pin_ = buzzer_pin;
        if (pin_ != -1) {
            pinMode(pin_, OUTPUT);
            digitalWrite(pin_, LOW);
            Serial.printf("BuzzerModule: Initialized on pin %d\n", pin_);
        } else {
            Serial.println(F("BuzzerModule: Not configured or pin is invalid."));
        }
    }

    void loop() {
        if (pin_ == -1) return;

        // Handle timed single beep (only when no sequence is playing)
        if (is_beeping_ && current_sequence_ == nullptr && millis() >= beep_end_time_) {
            stop();
        }

        // Handle note sequence playback
        if (current_sequence_ != nullptr && millis() >= sequence_next_time_) {
            if (sequence_index_ >= sequence_length_) {
                noTone(pin_);
                current_sequence_ = nullptr;
                return;
            }

            const Note& note = current_sequence_[sequence_index_];

            if (!sequence_playing_tone_) {
                tone(pin_, note.frequency);
                sequence_playing_tone_ = true;
                sequence_next_time_ = millis() + note.duration_ms;
            } else {
                noTone(pin_);
                sequence_playing_tone_ = false;
                sequence_next_time_ = millis() + note.pause_ms;
                sequence_index_++;
            }
        }

        // Handle mode triggers
        switch (current_mode) {
            case BUZZER_state::eINTRO:
                playSiren();
                current_mode = BUZZER_state::eOFF;
                break;
            case BUZZER_state::eGOAL:
                beep(1500, 200);
                current_mode = BUZZER_state::eOFF;
                break;
            case BUZZER_state::ePERIOD_BELL:
                startSequence(period_bell_sequence, PERIOD_BELL_LENGTH);
                current_mode = BUZZER_state::eOFF;
                break;
            case BUZZER_state::eALERT:
                startSequence(alert_sequence, ALERT_LENGTH);
                current_mode = BUZZER_state::eOFF;
                break;
            case BUZZER_state::eON:
            case BUZZER_state::eOFF:
            default:
                break;
        }
    }

    void beep(unsigned int frequency, unsigned int duration_ms) {
        if (pin_ == -1) return;
        tone(pin_, frequency);
        is_beeping_ = true;
        beep_end_time_ = millis() + duration_ms;
    }

    void beep(unsigned int duration_ms) {
        if (pin_ == -1) return;
        tone(pin_, 1000);
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
        if (new_mode != BUZZER_state::eON && new_mode != BUZZER_state::eOFF) {
            if (is_beeping_) stop();
            current_sequence_ = nullptr; // Cancel any playing sequence
        }
        current_mode = new_mode;
        if (new_mode == BUZZER_state::eOFF && pin_ != -1) {
            stop();
        }
    }

    void playSiren() {
        if (pin_ == -1) {
            Serial.println(F("BuzzerModule: Cannot play siren, pin not configured."));
            return;
        }
        startSequence(siren_sequence, SIREN_LENGTH);
    }

} // namespace BuzzerModule
