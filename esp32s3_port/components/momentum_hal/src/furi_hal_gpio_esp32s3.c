#include "furi_hal_gpio.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <stddef.h>

#define MTM_MAX_GPIO_ISR_HANDLERS 16

typedef struct {
    gpio_num_t pin;
    GpioExtiCallback callback;
    void* context;
    bool in_use;
} MtmGpioIsrSlot;

static const char* TAG = "mtm_gpio";
static MtmGpioIsrSlot s_isr_slots[MTM_MAX_GPIO_ISR_HANDLERS];
static bool s_isr_service_installed = false;

static void IRAM_ATTR mtm_gpio_isr_trampoline(void* arg) {
    MtmGpioIsrSlot* slot = (MtmGpioIsrSlot*)arg;
    if(slot->callback) {
        slot->callback(slot->context);
    }
}

static MtmGpioIsrSlot* mtm_gpio_find_slot(gpio_num_t pin) {
    for(int i = 0; i < MTM_MAX_GPIO_ISR_HANDLERS; i++) {
        if(s_isr_slots[i].in_use && s_isr_slots[i].pin == pin) {
            return &s_isr_slots[i];
        }
    }
    return NULL;
}

static MtmGpioIsrSlot* mtm_gpio_alloc_slot(gpio_num_t pin) {
    MtmGpioIsrSlot* existing = mtm_gpio_find_slot(pin);
    if(existing) return existing;
    for(int i = 0; i < MTM_MAX_GPIO_ISR_HANDLERS; i++) {
        if(!s_isr_slots[i].in_use) {
            s_isr_slots[i].in_use = true;
            s_isr_slots[i].pin = pin;
            return &s_isr_slots[i];
        }
    }
    ESP_LOGE(TAG, "no free GPIO ISR slots (max %d)", MTM_MAX_GPIO_ISR_HANDLERS);
    return NULL;
}

void furi_hal_gpio_init_simple(const GpioPin* gpio, GpioMode mode) {
    furi_hal_gpio_init(gpio, mode, GpioPullNo, GpioSpeedLow);
}

void furi_hal_gpio_init(const GpioPin* gpio, GpioMode mode, GpioPull pull, GpioSpeed speed) {
    (void)speed; // ESP32-S3 GPIO drive strength is a separate call
    // (gpio_set_drive_capability); not wired to Flipper's coarse
    // Low/Medium/High/VeryHigh enum yet - defaults are fine for buttons
    // and the SPI-driven display this port targets first.

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio->pin,
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = (pull == GpioPullUp) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (pull == GpioPullDown) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    switch(mode) {
    case GpioModeInput:
        cfg.mode = GPIO_MODE_INPUT;
        break;
    case GpioModeOutputPushPull:
    case GpioModeAltFunctionPushPull:
        cfg.mode = GPIO_MODE_OUTPUT;
        break;
    case GpioModeOutputOpenDrain:
    case GpioModeAltFunctionOpenDrain:
        cfg.mode = GPIO_MODE_OUTPUT_OD;
        break;
    case GpioModeAnalog:
        cfg.mode = GPIO_MODE_DISABLE;
        break;
    case GpioModeInterruptRise:
        cfg.mode = GPIO_MODE_INPUT;
        cfg.intr_type = GPIO_INTR_POSEDGE;
        break;
    case GpioModeInterruptFall:
        cfg.mode = GPIO_MODE_INPUT;
        cfg.intr_type = GPIO_INTR_NEGEDGE;
        break;
    case GpioModeInterruptRiseFall:
        cfg.mode = GPIO_MODE_INPUT;
        cfg.intr_type = GPIO_INTR_ANYEDGE;
        break;
    }

    ESP_ERROR_CHECK(gpio_config(&cfg));
}

void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioExtiCallback cb, void* ctx) {
    if(!s_isr_service_installed) {
        // ESP_INTR_FLAG_LOWMED: button-class GPIO interrupts, not
        // latency-critical enough to need level-1/NMI handling.
        esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
        if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(err);
        }
        s_isr_service_installed = true;
    }

    gpio_num_t pin = (gpio_num_t)gpio->pin;
    MtmGpioIsrSlot* slot = mtm_gpio_alloc_slot(pin);
    if(!slot) return;

    slot->callback = cb;
    slot->context = ctx;

    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, mtm_gpio_isr_trampoline, slot));
}

void furi_hal_gpio_enable_int_callback(const GpioPin* gpio) {
    gpio_intr_enable((gpio_num_t)gpio->pin);
}

void furi_hal_gpio_disable_int_callback(const GpioPin* gpio) {
    gpio_intr_disable((gpio_num_t)gpio->pin);
}

void furi_hal_gpio_remove_int_callback(const GpioPin* gpio) {
    gpio_num_t pin = (gpio_num_t)gpio->pin;
    gpio_isr_handler_remove(pin);
    MtmGpioIsrSlot* slot = mtm_gpio_find_slot(pin);
    if(slot) {
        slot->in_use = false;
        slot->callback = NULL;
        slot->context = NULL;
    }
}

void furi_hal_gpio_write(const GpioPin* gpio, bool state) {
    gpio_set_level((gpio_num_t)gpio->pin, state ? 1 : 0);
}

bool furi_hal_gpio_read(const GpioPin* gpio) {
    return gpio_get_level((gpio_num_t)gpio->pin) != 0;
}
