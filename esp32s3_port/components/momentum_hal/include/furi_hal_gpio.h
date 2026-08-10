/**
 * @file furi_hal_gpio.h
 *
 * ESP32-S3 replacement for the STM32 target's furi_hal_gpio.h.
 *
 * The original header wraps STM32 LL registers directly (GPIO_TypeDef,
 * BSRR/IDR, port+pin, an alternate-function mux table specific to the
 * STM32WB55's pinout) - none of that exists on ESP32-S3, which has a single
 * flat GPIO matrix instead of ported/muxed pins. This header keeps the same
 * *public API surface* (function names/signatures that furi.h and the rest
 * of the app layer call) so app code that only calls furi_hal_gpio_* keeps
 * compiling unmodified, but GpioPin now just wraps an ESP32 GPIO number and
 * the alt-fn/AF concept is dropped (ESP32-S3 peripherals are routed through
 * the GPIO matrix in software, not a fixed AF table - callers that need a
 * peripheral on a pin use the peripheral driver's own pin-assignment call,
 * e.g. spi_bus_config_t, not furi_hal_gpio_init_ex's alt_fn argument).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GpioExtiCallback)(void* ctx);

typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull,
    GpioModeOutputOpenDrain,
    GpioModeAltFunctionPushPull, // kept for API compat; treated as push-pull output
    GpioModeAltFunctionOpenDrain, // kept for API compat; treated as open-drain output
    GpioModeAnalog,
    GpioModeInterruptRise,
    GpioModeInterruptFall,
    GpioModeInterruptRiseFall,
} GpioMode;

typedef enum {
    GpioPullNo,
    GpioPullUp,
    GpioPullDown,
} GpioPull;

typedef enum {
    GpioSpeedLow,
    GpioSpeedMedium,
    GpioSpeedHigh,
    GpioSpeedVeryHigh,
} GpioSpeed;

/** ESP32-S3 GPIO number (0-48, board-dependent which are actually broken out) */
typedef struct {
    int32_t pin;
} GpioPin;

#define MTM_GPIO(num) \
    ((const GpioPin){.pin = (num)})

void furi_hal_gpio_init_simple(const GpioPin* gpio, GpioMode mode);
void furi_hal_gpio_init(const GpioPin* gpio, GpioMode mode, GpioPull pull, GpioSpeed speed);

void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioExtiCallback cb, void* ctx);
void furi_hal_gpio_enable_int_callback(const GpioPin* gpio);
void furi_hal_gpio_disable_int_callback(const GpioPin* gpio);
void furi_hal_gpio_remove_int_callback(const GpioPin* gpio);

void furi_hal_gpio_write(const GpioPin* gpio, bool state);
bool furi_hal_gpio_read(const GpioPin* gpio);

#ifdef __cplusplus
}
#endif
