#pragma once

#include <Arduino.h> // For standard types and functions like pinMode, digitalWrite, tone, noTone, millis, delay

namespace BuzzerModule
{
    // Enum for buzzer modes/states (can be used to trigger specific sounds)
    enum BUZZER_state {
        eOFF,        
        eON,         // For continuous sound (if desired, requires careful handling)
        eINTRO,      // Example: Play siren
        eGOAL,       // Example: Play goal sound
        ePERIOD_BELL // Example: Play period end sound
        // Add more states as needed for different sound events
    };


    // Function Declarations
    void init(int buzzer_pin);
    void loop(); // Handles timed beeps and possibly state-driven sounds like sirens
    void beep(unsigned int frequency, unsigned int duration_ms); // Beep with specific frequency and duration
    void beep(unsigned int duration_ms); // Beep with default frequency and specific duration
    void stop(); // Stops any current sound
    void playSiren(); // Plays a predefined siren sound (blocking)
    void setMode(BUZZER_state new_mode); // Sets the current mode, potentially triggering a sound in loop()


} // namespace BuzzerModule

