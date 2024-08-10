#pragma once

#include <stdint.h>

typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t auto_lock_with_pin;
    uint8_t display_clock;
} DesktopSettings;

void desktop_settings_load(DesktopSettings* settings);
void desktop_settings_save(const DesktopSettings* settings);
