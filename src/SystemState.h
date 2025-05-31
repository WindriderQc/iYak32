#pragma once

enum class SYS_state {
    BOOT,
    DEVICES_CONFIG,
    HEATUP,
    FIRSTLOOP,
    LOOP
};

// Accessor function declarations
SYS_state get_current_system_state_temp();
void set_current_system_state_temp(SYS_state new_state);
