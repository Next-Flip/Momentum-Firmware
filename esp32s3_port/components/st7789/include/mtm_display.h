/**
 * @file mtm_display.h
 *
 * ESP32-S3 + ST7789 (320x170) display driver.
 *
 * This is the hardware layer only. It does NOT yet plug into
 * applications/services/gui/canvas.c - that file is hardcoded to u8g2's
 * st756x (128x64, 1bpp) setup and drawing the wider color panel through it
 * needs its own adapter (converting/scaling u8g2's 1bpp framebuffer into
 * this driver's RGB565 one, or re-pointing canvas.c at a different
 * drawing backend entirely). That's tracked as Phase 2 in
 * PORTING_ESP32S3.md, not implemented here. What's here is enough to prove
 * the SPI/panel bring-up works and give app-layer porting something to
 * target next.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bring up SPI bus + ST7789 panel + backlight. Call once at boot. */
void mtm_display_init(void);

/** Fill the whole screen with a single RGB565 color. */
void mtm_display_clear(uint16_t color565);

/**
 * Blit a rectangular RGB565 pixel buffer to the panel.
 * @param x, y      top-left corner, in panel coordinates (0,0 .. 319,169)
 * @param w, h      dimensions of `pixels`
 * @param pixels    w*h RGB565 pixels, row-major
 */
void mtm_display_blit(int x, int y, int w, int h, const uint16_t* pixels);

/**
 * Convert-and-blit a 1bpp monochrome buffer (MSB-first, u8g2/Flipper
 * Canvas row-tiled layout: byte (x, y/8) bit (y%8), matching u8g2's
 * u8g2_uint_t tile buffer) onto the color panel, scaled by `scale` and
 * centered, using `fg`/`bg` as the two output colors. This is the intended
 * bridge point for canvas.c once it's ported (Phase 2) - exposed now so it
 * can be unit-exercised with a synthetic buffer before that lands.
 */
void mtm_display_blit_mono1(
    const uint8_t* mono_buf,
    int mono_w,
    int mono_h,
    int scale,
    uint16_t fg565,
    uint16_t bg565);

void mtm_display_set_backlight(bool on);

#ifdef __cplusplus
}
#endif
