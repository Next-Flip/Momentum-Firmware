#include "kiisu_light_adc.h"
#include <furi_hal.h>
#include <furi_hal_gpio.h>

#define KIISU_LIGHT_ADC_PIN gpio_ext_pc3
#define KIISU_LIGHT_ADC_CHANNEL FuriHalAdcChannel4
#define KIISU_LIGHT_ADC_MAX_MV 1187.0f

static bool adc_inited = false;

void kiisu_light_adc_init(void) {
    if(adc_inited) return;
    furi_hal_gpio_init(&KIISU_LIGHT_ADC_PIN, GpioModeAnalog, GpioPullDown, GpioSpeedHigh);
    adc_inited = true;
}

bool kiisu_light_adc_poll(KiisuLightAdcSample* out) {
    if(!adc_inited) kiisu_light_adc_init();
    FuriHalAdcHandle* adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(adc);
    uint16_t raw = furi_hal_adc_read(adc, KIISU_LIGHT_ADC_CHANNEL);
    float mv = furi_hal_adc_convert_to_voltage(adc, raw);
    furi_hal_adc_release(adc);
    out->voltage_mv = mv;
    out->percent = 100.0f - (mv / KIISU_LIGHT_ADC_MAX_MV * 100.0f);
    out->ok = (mv > 0.0f && mv < KIISU_LIGHT_ADC_MAX_MV * 1.2f);
    out->error_count = 0;
    return out->ok;
}
