/**
 * @file board_config.h
 *
 * Pin assignments for a genuine ESP32-S3-DevKitC-1 (N16R8) + 1.9" ST7789
 * 320x170 module wired on a breadboard. Still not flash-tested (no
 * hardware in the sandbox that wrote this), but the pins themselves are
 * chosen deliberately for this exact board, not arbitrary:
 *
 * - MOSI/SCLK/CS (11/12/10) are the chip's default hardware IOMUX pins for
 *   SPI2 on ESP32-S3 - using them (instead of routing through the GPIO
 *   matrix) gets the display its full SPI clock headroom "for free". DC/
 *   RST/BL (9/8/7) are adjacent, ordinary GPIOs with no special function.
 * - None of 7-12 collide with: the four strapping pins (0, 3, 45, 46 -
 *   sampled at boot, wiring anything else to them can prevent the board
 *   from booting/flashing), the native-USB pins (19, 20), the console
 *   UART (43, 44), or - specific to the N16R8 variant - GPIO 26-37, which
 *   its octal PSRAM uses internally and which are not broken out to
 *   headers on this board for exactly that reason.
 * - Buttons (4, 5, 6, 15, 16, 17) sit in the same safe range.
 *
 * If you're on a different board than an ESP32-S3-DevKitC-1, re-check all
 * of the above against its silkscreen/schematic - everything that touches
 * hardware pins reads from this one file.
 */
#pragma once

// --- ST7789 display (SPI2, default IOMUX pins) ---
#define MTM_LCD_PIN_MOSI 11
#define MTM_LCD_PIN_SCLK 12
#define MTM_LCD_PIN_CS   10
#define MTM_LCD_PIN_DC   9
#define MTM_LCD_PIN_RST  8
#define MTM_LCD_PIN_BL   7

#define MTM_LCD_WIDTH  320
#define MTM_LCD_HEIGHT 170
// Many 1.9" 320x170 ST7789 modules use a controller with 320x240 native
// RAM and the visible glass is offset within it - a "gap"/window offset is
// required or the image is shifted/cut off. 0/35 is the common value for
// this panel family but VARIES BY VENDOR - if the image is offset on your
// module, this is the first thing to change.
#define MTM_LCD_GAP_X 0
#define MTM_LCD_GAP_Y 35
#define MTM_LCD_MIRROR_X false
#define MTM_LCD_MIRROR_Y false
#define MTM_LCD_SWAP_XY false
#define MTM_LCD_INVERT_COLOR true

// --- Buttons ---
// Flipper Zero has 6 physical buttons (5-way d-pad + Back); this matches
// InputKey in applications/services/input/input.h.
#define MTM_BTN_PIN_UP    4
#define MTM_BTN_PIN_DOWN  5
#define MTM_BTN_PIN_LEFT  6
#define MTM_BTN_PIN_RIGHT 15
#define MTM_BTN_PIN_OK    16
#define MTM_BTN_PIN_BACK  17
// Set to true if your buttons pull the pin LOW when pressed (typical for a
// button wired to GND with internal pull-up enabled, which is what
// mtm_input assumes).
#define MTM_BTN_ACTIVE_LOW true
