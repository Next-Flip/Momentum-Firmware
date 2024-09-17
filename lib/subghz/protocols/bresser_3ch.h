#pragma once

#include <lib/subghz/protocols/base.h>

#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_BRESSER_3CH_NAME "Bresser-3CH"

typedef struct WSProtocolDecoderBresser3ch WSProtocolDecoderBresser3ch;
typedef struct WSProtocolEncoderBresser3ch WSProtocolEncoderBresser3ch;

extern const SubGhzProtocolDecoder ws_protocol_bresser_3ch_decoder;
extern const SubGhzProtocolEncoder ws_protocol_bresser_3ch_encoder;
extern const SubGhzProtocol ws_protocol_bresser_3ch;

/**
 * Allocate WSProtocolDecoderBresser3ch.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return WSProtocolDecoderBresser3ch* pointer to a WSProtocolDecoderBresser3ch instance
 */
void* ws_protocol_decoder_bresser_3ch_alloc(SubGhzEnvironment* environment);

/**
 * Free WSProtocolDecoderBresser3ch.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 */
void ws_protocol_decoder_bresser_3ch_free(void* context);

/**
 * Reset decoder WSProtocolDecoderBresser3ch.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 */
void ws_protocol_decoder_bresser_3ch_reset(void* context);

/**
 * Parse a raw sequence of levels and durations received from the air.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void ws_protocol_decoder_bresser_3ch_feed(void* context, bool level, uint32_t duration);

/**
 * Getting the hash sum of the last randomly received parcel.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 * @return hash Hash sum
 */
uint32_t ws_protocol_decoder_bresser_3ch_get_hash_data(void* context);

/**
 * Serialize data WSProtocolDecoderBresser3ch.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus ws_protocol_decoder_bresser_3ch_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data WSProtocolDecoderBresser3ch.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    ws_protocol_decoder_bresser_3ch_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a WSProtocolDecoderBresser3ch instance
 * @param output Resulting text
 */
void ws_protocol_decoder_bresser_3ch_get_string(void* context, FuriString* output);
