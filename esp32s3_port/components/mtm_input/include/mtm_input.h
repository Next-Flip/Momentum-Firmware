/**
 * @file mtm_input.h
 *
 * GPIO button driver for ESP32-S3, standing in for the full
 * applications/services/input service (not ported yet - that module pulls
 * in applications/services/storage, which needs its own filesystem/SD-card
 * bring-up first; see PORTING_ESP32S3.md Phase 2). InputKey below mirrors
 * targets/f7/furi_hal/furi_hal_resources.h's enum 1:1 by name so switching
 * callers over to the real furi InputEvent later is a rename, not a
 * redesign.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MtmInputKeyUp,
    MtmInputKeyDown,
    MtmInputKeyRight,
    MtmInputKeyLeft,
    MtmInputKeyOk,
    MtmInputKeyBack,
    MtmInputKeyMAX,
} MtmInputKey;

typedef enum {
    MtmInputTypePress,
    MtmInputTypeRelease,
    MtmInputTypeShort,
    MtmInputTypeLong,
    MtmInputTypeRepeat,
} MtmInputType;

typedef struct {
    MtmInputKey key;
    MtmInputType type;
} MtmInputEvent;

typedef void (*MtmInputCallback)(const MtmInputEvent* event, void* context);

/**
 * Configure button GPIOs (from board_config.h) and start the debounce/
 * long-press/repeat state machine on a dedicated FreeRTOS task.
 * `callback` is invoked from that task, not from ISR context.
 */
void mtm_input_init(MtmInputCallback callback, void* context);

#ifdef __cplusplus
}
#endif
