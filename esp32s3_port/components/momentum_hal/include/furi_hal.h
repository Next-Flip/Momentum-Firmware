/**
 * @file furi_hal.h
 *
 * Minimal furi_hal aggregator for the ESP32-S3 port.
 *
 * The STM32 target's furi_hal.h (targets/furi_hal_include/furi_hal.h) pulls
 * in ~20 headers (ADC, DMA, I2C, crypto, SD, region, speaker, GPIO bus
 * config, ...) that don't have ESP32-S3 implementations yet. furi/core only
 * needs the four below to compile and run its kernel/thread/memory
 * primitives (see PORTING_ESP32S3.md for how this was determined). Add more
 * headers here only as their .c implementations are actually written in
 * this component or a sibling one (st7789, mtm_input, ...) - don't add
 * declarations with no backing implementation.
 */
#pragma once

#include <furi_hal_cortex.h>
#include <furi_hal_memory.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
