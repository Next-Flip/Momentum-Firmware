#pragma once

#include "generic.h"

/**
 * Serialize common data SubGhzBlockGeneric (shared by other serialize methods).
 * @param protocol_name Pointer to a protocol name string
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return Status Error
 */
SubGhzProtocolStatus subghz_block_generic_serialize_common(
    const char* protocol_name,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
