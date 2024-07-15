#pragma once

#include "../blocks/generic_i.h"

#include <locale/locale.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS_NO_ID          0xFFFFFFFF
#define WS_NO_BATT        0xFF
#define WS_NO_HUMIDITY    0xFF
#define WS_NO_CHANNEL     0xFF
#define WS_NO_BTN         0xFF
#define WS_NO_TEMPERATURE -273.0f

typedef struct WSBlockGeneric WSBlockGeneric;

struct WSBlockGeneric {
    const char* protocol_name;
    uint64_t data;
    uint32_t id;
    uint8_t data_count_bit;
    uint8_t battery_low;
    uint8_t humidity;
    uint32_t timestamp;
    uint8_t channel;
    uint8_t btn;
    float temp;
};

/**
 * Serialize data WSBlockGeneric.
 * @param instance Pointer to a WSBlockGeneric instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus ws_block_generic_serialize(
    WSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data WSBlockGeneric.
 * @param instance Pointer to a WSBlockGeneric instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    ws_block_generic_deserialize(WSBlockGeneric* instance, FlipperFormat* flipper_format);

/**
 * Deserialize data WSBlockGeneric.
 * @param instance Pointer to a WSBlockGeneric instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param count_bit Count bit protocol
 * @return status
 */
SubGhzProtocolStatus ws_block_generic_deserialize_check_count_bit(
    WSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    uint16_t count_bit);

/**
 * Get string WSBlockGeneric.
 * @param instance Pointer to a WSBlockGeneric instance
 * @param output Pointer to a FuriString instance
 */
void ws_block_generic_get_string(WSBlockGeneric* instance, FuriString* output);

#ifdef __cplusplus
}
#endif
