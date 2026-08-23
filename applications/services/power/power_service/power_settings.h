#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PowerAutoPoweroffModeOff,
    PowerAutoPoweroffModeTimer,
    PowerAutoPoweroffModePercent,
} PowerAutoPoweroffMode;

typedef enum {
    PowerOffTimeout30,
    PowerOffTimeout60,
    PowerOffTimeout90,
} PowerOffTimeout;

#define POWER_AUTO_POWEROFF_PERCENT_STEP (5U)

typedef struct {
    PowerAutoPoweroffMode auto_poweroff_mode;
    uint32_t auto_poweroff_delay_ms;
    uint8_t charge_supress_percent;
    uint8_t auto_poweroff_percent;
    PowerOffTimeout power_off_timeout;
} PowerSettings;

void power_settings_load(PowerSettings* settings);
void power_settings_save(const PowerSettings* settings);
