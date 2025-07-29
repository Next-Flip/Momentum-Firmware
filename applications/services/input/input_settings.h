#pragma once

#include <stdint.h>

#define RECORD_INPUT_SETTINGS "input_settings"

typedef struct {
    uint8_t vibro_touch_level;
    uint8_t vibro_touch_trigger_mask;
} InputSettings;

void input_settings_load(InputSettings* settings);
void input_settings_save(const InputSettings* settings);
