#pragma once

enum class SYS_state {
    BOOT,
    DEVICES_CONFIG,
    HEATUP,
    FIRSTLOOP,
    LOOP
};

// Accessor function declarations
SYS_state get_current_system_state();
void set_current_system_state(SYS_state new_state);
