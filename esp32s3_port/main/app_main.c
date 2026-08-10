/**
 * Phase 1 bring-up entry point for the ESP32-S3 Momentum port.
 *
 * This does NOT boot the actual Momentum firmware (no applications/, no
 * GUI service, no scene manager, no desktop) - see PORTING_ESP32S3.md for
 * why that's a much larger, separate effort. What this proves is the
 * bottom of the stack: furi/core's kernel primitives running on ESP-IDF's
 * FreeRTOS, the ST7789 panel driver, and the button driver, wired together
 * end to end. A square you can drive around the screen with the d-pad is
 * the whole "app" - it exists to make the input->render loop visible on
 * real hardware once someone builds and flashes this, not as a feature.
 */
#include "board_config.h"
#include "furi.h"
#include "mtm_display.h"
#include "mtm_input.h"

#include "esp_log.h"

#include <string.h>

static const char* TAG = "app_main";

#define CURSOR_SIZE 16
#define CURSOR_STEP 8

static int s_cursor_x = (MTM_LCD_WIDTH - CURSOR_SIZE) / 2;
static int s_cursor_y = (MTM_LCD_HEIGHT - CURSOR_SIZE) / 2;
static uint16_t s_cursor_color = 0xFFFF; // white (RGB565)
static uint16_t s_prev_x = 0, s_prev_y = 0;
static bool s_have_prev = false;

static uint16_t s_cursor_buf[CURSOR_SIZE * CURSOR_SIZE];
static uint16_t s_erase_buf[CURSOR_SIZE * CURSOR_SIZE];

static void redraw_cursor(void) {
    if(s_have_prev) {
        mtm_display_blit(s_prev_x, s_prev_y, CURSOR_SIZE, CURSOR_SIZE, s_erase_buf);
    }

    for(int i = 0; i < CURSOR_SIZE * CURSOR_SIZE; i++) {
        s_cursor_buf[i] = s_cursor_color;
    }
    mtm_display_blit(s_cursor_x, s_cursor_y, CURSOR_SIZE, CURSOR_SIZE, s_cursor_buf);

    s_prev_x = s_cursor_x;
    s_prev_y = s_cursor_y;
    s_have_prev = true;
}

static void on_input(const MtmInputEvent* event, void* context) {
    (void)context;
    if(event->type != MtmInputTypePress && event->type != MtmInputTypeRepeat) {
        return;
    }

    switch(event->key) {
    case MtmInputKeyUp:
        s_cursor_y -= CURSOR_STEP;
        break;
    case MtmInputKeyDown:
        s_cursor_y += CURSOR_STEP;
        break;
    case MtmInputKeyLeft:
        s_cursor_x -= CURSOR_STEP;
        break;
    case MtmInputKeyRight:
        s_cursor_x += CURSOR_STEP;
        break;
    case MtmInputKeyOk:
        // RGB565 cycle: white -> red -> green -> blue -> white
        if(s_cursor_color == 0xFFFF)
            s_cursor_color = 0xF800;
        else if(s_cursor_color == 0xF800)
            s_cursor_color = 0x07E0;
        else if(s_cursor_color == 0x07E0)
            s_cursor_color = 0x001F;
        else
            s_cursor_color = 0xFFFF;
        break;
    case MtmInputKeyBack:
        mtm_display_clear(0x0000);
        s_have_prev = false;
        break;
    default:
        break;
    }

    if(s_cursor_x < 0) s_cursor_x = 0;
    if(s_cursor_y < 0) s_cursor_y = 0;
    if(s_cursor_x > MTM_LCD_WIDTH - CURSOR_SIZE) s_cursor_x = MTM_LCD_WIDTH - CURSOR_SIZE;
    if(s_cursor_y > MTM_LCD_HEIGHT - CURSOR_SIZE) s_cursor_y = MTM_LCD_HEIGHT - CURSOR_SIZE;

    redraw_cursor();
}

void app_main(void) {
    ESP_LOGI(TAG, "Momentum ESP32-S3 port - Phase 1 bring-up");

    // furi_init() sets up furi/core's thread registry, logging, and record
    // (pubsub-by-name) systems. Unlike the STM32 target's main.c, we do NOT
    // call furi_run() -> vTaskStartScheduler(): ESP-IDF already started the
    // FreeRTOS scheduler before invoking app_main(), which itself runs as
    // a task, not pre-scheduler boot code.
    furi_init();

    mtm_display_init();
    memset(s_erase_buf, 0x00, sizeof(s_erase_buf));

    mtm_input_init(on_input, NULL);

    redraw_cursor();

    ESP_LOGI(TAG, "Bring-up loop running. D-pad moves the square, OK cycles color, Back clears.");
}
