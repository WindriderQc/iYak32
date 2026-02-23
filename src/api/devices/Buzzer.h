#pragma once

#include <Arduino.h>

namespace BuzzerModule
{
    enum BUZZER_state {
        eOFF,
        eON,
        eINTRO,
        eGOAL,
        ePERIOD_BELL,
        eALERT       // 3 short high beeps for error notifications
    };

    struct Note {
        unsigned int frequency;
        unsigned int duration_ms;
        unsigned int pause_ms;
    };

    void init(int buzzer_pin);
    void loop();
    void beep(unsigned int frequency, unsigned int duration_ms);
    void beep(unsigned int duration_ms);
    void stop();
    void playSiren();
    void setMode(BUZZER_state new_mode);

} // namespace BuzzerModule
