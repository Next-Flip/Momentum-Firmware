#pragma once

#include <lib/subghz/protocols/base.h>

#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_SOLIGHT_TE44_NAME "Solight TE44"

typedef struct WSProtocolDecoderSolightTE44 WSProtocolDecoderSolightTE44;
typedef struct WSProtocolEncoderSolightTE44 WSProtocolEncoderSolightTE44;

extern const SubGhzProtocolDecoder ws_protocol_solight_te44_decoder;
extern const SubGhzProtocolEncoder ws_protocol_solight_te44_encoder;
extern const SubGhzProtocol ws_protocol_solight_te44;

/**
 * Allocate WSProtocolDecoderSolightTE44.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return WSProtocolDecoderSolightTE44* pointer to a WSProtocolDecoderSolightTE44 instance
 */
void* ws_protocol_decoder_solight_te44_alloc(SubGhzEnvironment* environment);

/**
 * Free WSProtocolDecoderSolightTE44.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 */
void ws_protocol_decoder_solight_te44_free(void* context);

/**
 * Reset decoder WSProtocolDecoderSolightTE44.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 */
void ws_protocol_decoder_solight_te44_reset(void* context);

/**
 * Parse a raw sequence of levels and durations received from the air.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void ws_protocol_decoder_solight_te44_feed(void* context, bool level, uint32_t duration);

/**
 * Getting the hash sum of the last randomly received parcel.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 * @return hash Hash sum
 */
uint32_t ws_protocol_decoder_solight_te44_get_hash_data(void* context);

/**
 * Serialize data WSProtocolDecoderSolightTE44.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus ws_protocol_decoder_solight_te44_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data WSProtocolDecoderSolightTE44.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    ws_protocol_decoder_solight_te44_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a WSProtocolDecoderSolightTE44 instance
 * @param output Resulting text
 */
void ws_protocol_decoder_solight_te44_get_string(void* context, FuriString* output);
