#pragma once

#include <stdint.h>

typedef struct {
    uint32_t shutdown_idle_delay_ms;
} PowerSettings;

void power_settings_load(PowerSettings* settings);
void power_settings_save(const PowerSettings* settings);
