#include "mtm_input.h"
#include "board_config.h"
#include "furi_hal_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Polling rather than GPIO-interrupt driven: simpler to get right without
// hardware to tune against (see PORTING_ESP32S3.md), and six buttons
// polled every 10ms is negligible CPU cost. furi_hal_gpio_add_int_callback
// exists (momentum_hal) if this needs to move to interrupts later.
#define MTM_INPUT_POLL_MS        10
#define MTM_INPUT_DEBOUNCE_POLLS 3 // ~30ms of consistent readings before accepting a state change
#define MTM_INPUT_LONG_PRESS_MS  350
#define MTM_INPUT_REPEAT_MS      100

typedef struct {
    GpioPin gpio;
    MtmInputKey key;
    bool stable_pressed;
    bool raw_pressed;
    uint8_t debounce_count;
    uint32_t press_start_ms;
    uint32_t last_repeat_ms;
    bool long_sent;
} MtmButtonState;

static MtmButtonState s_buttons[MtmInputKeyMAX] = {
    [MtmInputKeyUp] = {.gpio = {MTM_BTN_PIN_UP}, .key = MtmInputKeyUp},
    [MtmInputKeyDown] = {.gpio = {MTM_BTN_PIN_DOWN}, .key = MtmInputKeyDown},
    [MtmInputKeyRight] = {.gpio = {MTM_BTN_PIN_RIGHT}, .key = MtmInputKeyRight},
    [MtmInputKeyLeft] = {.gpio = {MTM_BTN_PIN_LEFT}, .key = MtmInputKeyLeft},
    [MtmInputKeyOk] = {.gpio = {MTM_BTN_PIN_OK}, .key = MtmInputKeyOk},
    [MtmInputKeyBack] = {.gpio = {MTM_BTN_PIN_BACK}, .key = MtmInputKeyBack},
};

static MtmInputCallback s_callback = NULL;
static void* s_context = NULL;

static void mtm_input_emit(MtmInputKey key, MtmInputType type) {
    if(s_callback) {
        MtmInputEvent evt = {.key = key, .type = type};
        s_callback(&evt, s_context);
    }
}

static void mtm_input_task(void* arg) {
    (void)arg;
    for(;;) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        for(int i = 0; i < MtmInputKeyMAX; i++) {
            MtmButtonState* b = &s_buttons[i];
            bool level = furi_hal_gpio_read(&b->gpio);
            bool pressed_now = MTM_BTN_ACTIVE_LOW ? !level : level;

            if(pressed_now != b->raw_pressed) {
                b->raw_pressed = pressed_now;
                b->debounce_count = 0;
            } else if(b->debounce_count < MTM_INPUT_DEBOUNCE_POLLS) {
                b->debounce_count++;
            }

            bool debounced = (b->debounce_count >= MTM_INPUT_DEBOUNCE_POLLS) ? b->raw_pressed :
                                                                                b->stable_pressed;

            if(debounced != b->stable_pressed) {
                b->stable_pressed = debounced;
                if(b->stable_pressed) {
                    b->press_start_ms = now_ms;
                    b->long_sent = false;
                    mtm_input_emit(b->key, MtmInputTypePress);
                } else {
                    if(!b->long_sent) {
                        mtm_input_emit(b->key, MtmInputTypeShort);
                    }
                    mtm_input_emit(b->key, MtmInputTypeRelease);
                }
            } else if(b->stable_pressed) {
                uint32_t held_ms = now_ms - b->press_start_ms;
                if(!b->long_sent && held_ms >= MTM_INPUT_LONG_PRESS_MS) {
                    b->long_sent = true;
                    b->last_repeat_ms = now_ms;
                    mtm_input_emit(b->key, MtmInputTypeLong);
                } else if(b->long_sent && (now_ms - b->last_repeat_ms) >= MTM_INPUT_REPEAT_MS) {
                    b->last_repeat_ms = now_ms;
                    mtm_input_emit(b->key, MtmInputTypeRepeat);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MTM_INPUT_POLL_MS));
    }
}

void mtm_input_init(MtmInputCallback callback, void* context) {
    s_callback = callback;
    s_context = context;

    for(int i = 0; i < MtmInputKeyMAX; i++) {
        furi_hal_gpio_init(
            &s_buttons[i].gpio,
            GpioModeInput,
            MTM_BTN_ACTIVE_LOW ? GpioPullUp : GpioPullDown,
            GpioSpeedLow);
    }

    xTaskCreate(mtm_input_task, "mtm_input", 2048, NULL, 5, NULL);
}
