/**
 * @file board_config.h
 *
 * Pin assignments for the ESP32-S3-N16R8 + 1.9" ST7789 320x170 board this
 * port targets. These are NOT verified against real hardware (this
 * environment has no ESP32-S3 attached to flash-test against) - they're a
 * conventional, commonly-used assignment for ESP32-S3-DevKitC-1-style
 * boards paired with this exact display module (same SPI panel used by
 * Sor3nt's ESP32 Flipper port and most "cheap ESP32-S3 1.9in TFT" kits).
 * Cross-check against your specific board's silkscreen/schematic before
 * flashing, and adjust here - everything that touches hardware pins reads
 * from this one file.
 */
#pragma once

// --- ST7789 display (SPI2/HSPI) ---
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
