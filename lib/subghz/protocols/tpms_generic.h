#pragma once

#include "../blocks/generic_i.h"

#include <locale/locale.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TPMS_NO_BATT 0xFF

typedef struct TPMSBlockGeneric TPMSBlockGeneric;

struct TPMSBlockGeneric {
    const char* protocol_name;
    uint64_t data;
    uint8_t data_count_bit;

    uint32_t timestamp;

    uint32_t id;
    uint8_t battery_low;
    // bool storage;
    float pressure; // bar
    float temperature; // celsius
};

/**
     * Serialize data TPMSBlockGeneric.
     * @param instance Pointer to a TPMSBlockGeneric instance
     * @param flipper_format Pointer to a FlipperFormat instance
     * @param preset The modulation on which the signal was received, SubGhzRadioPreset
     * @return status
     */
SubGhzProtocolStatus tpms_block_generic_serialize(
    TPMSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
     * Deserialize data TPMSBlockGeneric.
     * @param instance Pointer to a TPMSBlockGeneric instance
     * @param flipper_format Pointer to a FlipperFormat instance
     * @return status
     */
SubGhzProtocolStatus
    tpms_block_generic_deserialize(TPMSBlockGeneric* instance, FlipperFormat* flipper_format);

/**
     * Deserialize data TPMSBlockGeneric.
     * @param instance Pointer to a TPMSBlockGeneric instance
     * @param flipper_format Pointer to a FlipperFormat instance
     * @param count_bit Count bit protocol
     * @return status
     */
SubGhzProtocolStatus tpms_block_generic_deserialize_check_count_bit(
    TPMSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    uint16_t count_bit);

#ifdef __cplusplus
}
#endif
