#pragma once

#include <lib/subghz/protocols/base.h>

#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_VAUNO_EN8822C_NAME "Vauno-EN8822C"

typedef struct WSProtocolDecoderVaunoEN8822C WSProtocolDecoderVaunoEN8822C;
typedef struct WSProtocolEncoderVaunoEN8822C WSProtocolEncoderVaunoEN8822C;

extern const SubGhzProtocolDecoder ws_protocol_vauno_en8822c_decoder;
extern const SubGhzProtocolEncoder ws_protocol_vauno_en8822c_encoder;
extern const SubGhzProtocol ws_protocol_vauno_en8822c;

/**
 * Allocate WSProtocolDecoderVaunoEN8822C.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return WSProtocolDecoderVaunoEN8822C* pointer to a WSProtocolDecoderVaunoEN8822C instance
 */
void* ws_protocol_decoder_vauno_en8822c_alloc(SubGhzEnvironment* environment);

/**
 * Free WSProtocolDecoderVaunoEN8822C.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 */
void ws_protocol_decoder_vauno_en8822c_free(void* context);

/**
 * Reset decoder WSProtocolDecoderVaunoEN8822C.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 */
void ws_protocol_decoder_vauno_en8822c_reset(void* context);

/**
 * Parse a raw sequence of levels and durations received from the air.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void ws_protocol_decoder_vauno_en8822c_feed(void* context, bool level, uint32_t duration);

/**
 * Getting the hash sum of the last randomly received parcel.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 * @return hash Hash sum
 */
uint32_t ws_protocol_decoder_vauno_en8822c_get_hash_data(void* context);

/**
 * Serialize data WSProtocolDecoderVaunoEN8822C.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus ws_protocol_decoder_vauno_en8822c_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data WSProtocolDecoderVaunoEN8822C.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    ws_protocol_decoder_vauno_en8822c_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a WSProtocolDecoderVaunoEN8822C instance
 * @param output Resulting text
 */
void ws_protocol_decoder_vauno_en8822c_get_string(void* context, FuriString* output);
