#include "vauno_en8822c.h"
#include "furi/core/log.h"
#define TAG "WSProtocolVaunoEN8822C"

/*
 * Help
 * https://github.com/merbanan/rtl_433/blob/master/src/devices/vauno_en8822c.c
 *
 * Vauno EN8822C sensor on 433.92MHz.
 *
 * Largely the same as Esperanza EWS, s3318p.
 * @sa esperanza_ews.c s3318p.c

 * List of known supported devices:
 * - Vauno EN8822C-1
 * - FUZHOU ESUN ELECTRONIC outdoor T21 sensor
 *
 * Frame structure (42 bits):
 *
 *    Byte:      0        1        2        3        4
 *    Nibble:    1   2    3   4    5   6    7   8    9   10   11
 *    Type:      IIIIIIII B?CCTTTT TTTTTTTT HHHHHHHF FFFBXXXX XX
 *
 * - I: Random device ID
 * - C: Channel (1-3)
 * - T: Temperature (Little-endian)
 * - H: Humidity (Little-endian)
 * - F: Flags (unknown)
 * - B: Battery (1=low voltage ~<2.5V)
 * - X: Checksum (6 bit nibble sum)
 * 
 * Sample Data:
 * 
 *     [00] {42} af 0f a2 7c 01 c0 : 10101111 00001111 10100010 01111100 00000001 11
 * 
 * - Sensor ID = 175 = 0xaf
 * - Channel = 0
 * - temp = -93 = 0x111110100010
 * - TemperatureC = -9.3
 * - hum = 62% = 0x0111110
 *
 * Copyright (C) 2022 Jamie Barron <gumbald@gmail.com>
 *
 * @m7i-org - because the ether is wavy
 *
 */

static const SubGhzBlockConst ws_protocol_vauno_en8822c_const = {
    .te_short = 500,
    .te_long = 1940,
    .te_delta = 150,
    .min_count_bit_for_found = 42,
};

struct WSProtocolDecoderVaunoEN8822C {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
};

struct WSProtocolEncoderVaunoEN8822C {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

const SubGhzProtocolDecoder ws_protocol_vauno_en8822c_decoder = {
    .alloc = ws_protocol_decoder_vauno_en8822c_alloc,
    .free = ws_protocol_decoder_vauno_en8822c_free,

    .feed = ws_protocol_decoder_vauno_en8822c_feed,
    .reset = ws_protocol_decoder_vauno_en8822c_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = ws_protocol_decoder_vauno_en8822c_get_hash_data,
    .serialize = ws_protocol_decoder_vauno_en8822c_serialize,
    .deserialize = ws_protocol_decoder_vauno_en8822c_deserialize,
    .get_string = ws_protocol_decoder_vauno_en8822c_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder ws_protocol_vauno_en8822c_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_vauno_en8822c = {
    .name = WS_PROTOCOL_VAUNO_EN8822C_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_868 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save,
    .filter = SubGhzProtocolFilter_Weather,
    .decoder = &ws_protocol_vauno_en8822c_decoder,
    .encoder = &ws_protocol_vauno_en8822c_encoder,
};

typedef enum {
    VaunoEN8822CDecoderStepReset = 0,
    VaunoEN8822CDecoderStepSaveDuration,
    VaunoEN8822CDecoderStepCheckDuration,
} VaunoEN8822CDecoderStep;

void* ws_protocol_decoder_vauno_en8822c_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    WSProtocolDecoderVaunoEN8822C* instance = malloc(sizeof(WSProtocolDecoderVaunoEN8822C));
    instance->base.protocol = &ws_protocol_vauno_en8822c;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void ws_protocol_decoder_vauno_en8822c_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    free(instance);
}

void ws_protocol_decoder_vauno_en8822c_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
}

static bool ws_protocol_vauno_en8822c_check(WSProtocolDecoderVaunoEN8822C* instance) {
    if(!instance->decoder.decode_data) return false;

    // The sum of all nibbles should match the last 6 bits
    uint8_t sum = 0;
    for(uint8_t i = 6; i <= 38; i += 4) {
        sum += ((instance->decoder.decode_data >> i) & 0x0f);
    }

    return sum != 0 && (sum & 0x3f) == (instance->decoder.decode_data & 0x3f);
}

/**
 * Analysis of received data
 * @param instance Pointer to a WSBlockGeneric* instance
 */
static void ws_protocol_vauno_en8822c_extract_data(WSBlockGeneric* instance) {
    instance->id = (instance->data >> 34) & 0xff;
    instance->battery_low = (instance->data >> 33) & 0x01;
    instance->channel = ((instance->data >> 30) & 0x03);

    int16_t temp = (instance->data >> 18) & 0x0fff;
    /* Handle signed data */
    if(temp & 0x0800) {
        temp |= 0xf000;
    }
    instance->temp = (float)temp / 10.0;

    instance->humidity = (instance->data >> 11) & 0x7f;
    instance->btn = WS_NO_BTN;
}

void ws_protocol_decoder_vauno_en8822c_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;

    switch(instance->decoder.parser_step) {
    case VaunoEN8822CDecoderStepReset:
        if((!level) && DURATION_DIFF(duration, ws_protocol_vauno_en8822c_const.te_long * 4) <
                           ws_protocol_vauno_en8822c_const.te_delta) {
            instance->decoder.parser_step = VaunoEN8822CDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case VaunoEN8822CDecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = VaunoEN8822CDecoderStepCheckDuration;
        } else {
            instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
        }
        break;

    case VaunoEN8822CDecoderStepCheckDuration:
        if(!level) {
            if(DURATION_DIFF(instance->decoder.te_last, ws_protocol_vauno_en8822c_const.te_short) <
               ws_protocol_vauno_en8822c_const.te_delta) {
                if(DURATION_DIFF(duration, ws_protocol_vauno_en8822c_const.te_long * 2) <
                   ws_protocol_vauno_en8822c_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                    instance->decoder.parser_step = VaunoEN8822CDecoderStepSaveDuration;
                } else if(
                    DURATION_DIFF(duration, ws_protocol_vauno_en8822c_const.te_long) <
                    ws_protocol_vauno_en8822c_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                    instance->decoder.parser_step = VaunoEN8822CDecoderStepSaveDuration;
                } else if(
                    DURATION_DIFF(duration, ws_protocol_vauno_en8822c_const.te_long * 4) <
                    ws_protocol_vauno_en8822c_const.te_delta) {
                    instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
                    if(instance->decoder.decode_count_bit ==
                           ws_protocol_vauno_en8822c_const.min_count_bit_for_found &&
                       ws_protocol_vauno_en8822c_check(instance)) {
                        instance->generic.data = instance->decoder.decode_data;
                        instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                        ws_protocol_vauno_en8822c_extract_data(&instance->generic);

                        if(instance->base.callback) {
                            instance->base.callback(&instance->base, instance->base.context);
                        }
                    } else if(instance->decoder.decode_count_bit == 1) {
                        instance->decoder.parser_step = VaunoEN8822CDecoderStepSaveDuration;
                    }

                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 0;
                } else
                    instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
            } else
                instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
        } else
            instance->decoder.parser_step = VaunoEN8822CDecoderStepReset;
        break;
    }
}

uint32_t ws_protocol_decoder_vauno_en8822c_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_vauno_en8822c_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    ws_protocol_decoder_vauno_en8822c_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    return ws_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        ws_protocol_vauno_en8822c_const.min_count_bit_for_found);
}

void ws_protocol_decoder_vauno_en8822c_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderVaunoEN8822C* instance = context;
    ws_block_generic_get_string(&instance->generic, output);
}
