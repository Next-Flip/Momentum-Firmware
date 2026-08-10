#include "mtm_display.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include <stdlib.h>

static const char* TAG = "mtm_display";
static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;

void mtm_display_set_backlight(bool on) {
    gpio_set_level(MTM_LCD_PIN_BL, on ? 1 : 0);
}

void mtm_display_init(void) {
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << MTM_LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    mtm_display_set_backlight(false); // stay off until the first clear() lands, avoids a garbage-frame flash

    spi_bus_config_t buscfg = {
        .sclk_io_num = MTM_LCD_PIN_SCLK,
        .mosi_io_num = MTM_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // One 80-row DMA chunk's worth of RGB565; draw_bitmap() below is
        // called one scanline at a time for now (see TODO there) so this
        // is generous headroom, not a hard requirement.
        .max_transfer_sz = MTM_LCD_WIDTH * 80 * (int)sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = MTM_LCD_PIN_DC,
        .cs_gpio_num = MTM_LCD_PIN_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // NOTE: esp_lcd_panel_io_spi_config_t's field names have shifted a bit
    // across ESP-IDF 5.x minor versions (particularly around color-order
    // config, which lives on esp_lcd_panel_dev_config_t below, not here in
    // some releases). If this doesn't compile against the IDF version you
    // install, diff this struct against
    // components/esp_lcd/include/esp_lcd_panel_io.h for that version -
    // this was written from the v5.1/v5.2-era shape without a toolchain
    // available to verify against (see PORTING_ESP32S3.md).
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &s_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MTM_LCD_PIN_RST,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, MTM_LCD_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, MTM_LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, MTM_LCD_MIRROR_X, MTM_LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, MTM_LCD_GAP_X, MTM_LCD_GAP_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    mtm_display_clear(0x0000);
    mtm_display_set_backlight(true);
    ESP_LOGI(TAG, "ST7789 %dx%d up", MTM_LCD_WIDTH, MTM_LCD_HEIGHT);
}

void mtm_display_blit(int x, int y, int w, int h, const uint16_t* pixels) {
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, pixels);
}

void mtm_display_clear(uint16_t color565) {
    // TODO(perf): this pushes one scanline per SPI transaction, which is
    // plenty to prove bring-up but leaves DMA mostly idle. A real GUI
    // integration (Phase 2) should keep a full 320x170 framebuffer and
    // push it in one (or a few large) draw_bitmap() calls instead.
    static uint16_t line[MTM_LCD_WIDTH];
    for(int x = 0; x < MTM_LCD_WIDTH; x++) {
        line[x] = color565;
    }
    for(int y = 0; y < MTM_LCD_HEIGHT; y++) {
        mtm_display_blit(0, y, MTM_LCD_WIDTH, 1, line);
    }
}

void mtm_display_blit_mono1(
    const uint8_t* mono_buf,
    int mono_w,
    int mono_h,
    int scale,
    uint16_t fg565,
    uint16_t bg565) {
    if(scale < 1) scale = 1;
    int out_w = mono_w * scale;
    int out_h = mono_h * scale;

    int off_x = (MTM_LCD_WIDTH - out_w) / 2;
    int off_y = (MTM_LCD_HEIGHT - out_h) / 2;
    if(off_x < 0) off_x = 0;
    if(off_y < 0) off_y = 0;

    uint16_t* row = heap_caps_malloc(out_w * sizeof(uint16_t), MALLOC_CAP_DMA);
    if(row == NULL) {
        ESP_LOGE(TAG, "blit_mono1: out of DMA-capable memory for %d px row", out_w);
        return;
    }

    // u8g2's tile buffer layout (matches what canvas.c's u8g2 framebuffer
    // uses): buf[(y/8) * mono_w + x], pixel set if bit (y%8) is 1, LSB
    // first. Unverified against a live canvas.c integration - see the
    // header comment on this function.
    for(int sy = 0; sy < mono_h; sy++) {
        int tile_row = sy / 8;
        int bit = sy % 8;
        const uint8_t* src_row = &mono_buf[tile_row * mono_w];

        for(int sx = 0; sx < mono_w; sx++) {
            bool set = (src_row[sx] >> bit) & 1;
            uint16_t color = set ? fg565 : bg565;
            for(int r = 0; r < scale; r++) {
                row[sx * scale + r] = color;
            }
        }

        for(int r = 0; r < scale; r++) {
            mtm_display_blit(off_x, off_y + sy * scale + r, out_w, 1, row);
        }
    }

    free(row);
}
