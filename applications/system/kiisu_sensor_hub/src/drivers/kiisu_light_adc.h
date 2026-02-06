#pragma once
#include <furi_hal.h>
#include <stdint.h>

typedef struct {
    float voltage_mv;
    float percent;
    bool ok;
    uint32_t error_count;
} KiisuLightAdcSample;

void kiisu_light_adc_init(void);
bool kiisu_light_adc_poll(KiisuLightAdcSample* out);
