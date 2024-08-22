#include "solight_te44.h"
#include "furi/core/log.h"
#define TAG "WSProtocolSolightTE44"

/*
 * Help
 * https://github.com/merbanan/rtl_433/blob/master/src/devices/solight_te44.c
 *
 * Solight TE44 -- Generic wireless thermometer, which might be sold as part of different kits.
 *
 * So far these were identified (mostly sold in central/eastern europe)
 * - Solight TE44
 * - Solight TE66
 * - EMOS E0107T
 * - NX-6876-917 from Pearl (for FWS-70 station).
 * - newer TFA 30.3197
 *
 * Rated -50 C to 70 C, frequency 433,92 MHz, three selectable channels.
 *
 * Encoding (see in applications/debug/unit_tests/resources/unit_tests/subghz/solight_te44_raw.sub for example)
 * - Data begins with     500 -3890
 * - A zero is encoded    500 -960
 * - A one is encoded     500 -1940
 * - Repetition ends with 500 -470  500 -3890
 *
 * Data structure:
 *
 * - 12 repetitions of the same 36 bit payload, a sync between each repetition
 * - 36 bit payload format: IIII IIII B0cc TTTT TTTT TTTT 1111 cccc cccc
 *                                  ^ ^  ^              ^              ^
 *                                  | |  |              |              |
 *        8 bit ID (resets) at 28--/  |  |              |              |
 *        1 bit battery flag  at 27--/  /               |              |
 *        2 bit channel (0-2) at 24 ---/               /               |
 *        12 bit signed int temp(C) scale 10 at 12----/               /
 *        8 bit checksum --------------------------------------------/
 *
 * If you're looking at this and thinking: this looks a lot like the Auriol HG06061A data structure. You're not alone,
 * this is almost identical except for the last checksum, which is not only absent in the Auriol structure, but
 * the same bits encode the humidity value. So maybe this explains older results of some random weather stations with
 * implausible humidity values. By adding this before Auriol and expecting the weird data sync we avoid conflicts
 * with its decoder.
 *
 * @m7i-org - because I want less BinRAW in my life
 *
 */

static const SubGhzBlockConst ws_protocol_solight_te44_const = {
    .te_short = 490,
    .te_long = 3000,
    .te_delta = 150,
    .min_count_bit_for_found = 36,
};

struct WSProtocolDecoderSolightTE44 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
};

struct WSProtocolEncoderSolightTE44 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

const SubGhzProtocolDecoder ws_protocol_solight_te44_decoder = {
    .alloc = ws_protocol_decoder_solight_te44_alloc,
    .free = ws_protocol_decoder_solight_te44_free,

    .feed = ws_protocol_decoder_solight_te44_feed,
    .reset = ws_protocol_decoder_solight_te44_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = ws_protocol_decoder_solight_te44_get_hash_data,
    .serialize = ws_protocol_decoder_solight_te44_serialize,
    .deserialize = ws_protocol_decoder_solight_te44_deserialize,
    .get_string = ws_protocol_decoder_solight_te44_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder ws_protocol_solight_te44_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_solight_te44 = {
    .name = WS_PROTOCOL_SOLIGHT_TE44_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_868 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save,
    .filter = SubGhzProtocolFilter_Weather,
    .decoder = &ws_protocol_solight_te44_decoder,
    .encoder = &ws_protocol_solight_te44_encoder,
};

typedef enum {
    SolightTE44DecoderStepReset = 0,
    SolightTE44DecoderStepSaveDuration,
    SolightTE44DecoderStepCheckDuration,
} SolightTE44DecoderStep;

void* ws_protocol_decoder_solight_te44_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    WSProtocolDecoderSolightTE44* instance = malloc(sizeof(WSProtocolDecoderSolightTE44));
    instance->base.protocol = &ws_protocol_solight_te44;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void ws_protocol_decoder_solight_te44_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    free(instance);
}

void ws_protocol_decoder_solight_te44_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    instance->decoder.parser_step = SolightTE44DecoderStepReset;
}

static bool ws_protocol_solight_te44_check(WSProtocolDecoderSolightTE44* instance) {
    if(!instance->decoder.decode_data) return false;
    if(((instance->decoder.decode_data >> 8) & 0x0f) != 0x0f) return false; // const not 1111

    // Rubicson CRC check
    uint8_t msg_rubicson_crc[] = {
        instance->decoder.decode_data >> 28,
        instance->decoder.decode_data >> 20,
        instance->decoder.decode_data >> 12,
        0xf0,
        (instance->decoder.decode_data & 0xf0) | (instance->decoder.decode_data & 0x0f)};

    uint8_t rubicson_crc = subghz_protocol_blocks_crc8(msg_rubicson_crc, 5, 0x31, 0x6c);

    return rubicson_crc == 0;
}

/**
 * Analysis of received data
 * @param instance Pointer to a WSBlockGeneric* instance
 */
static void ws_protocol_solight_te44_extract_data(WSBlockGeneric* instance) {
    instance->id = (instance->data >> 28) & 0xff;
    instance->battery_low = !(instance->data >> 27) & 0x01;
    instance->channel = ((instance->data >> 24) & 0x03) + 1;

    int16_t temp = (instance->data >> 12) & 0x0fff;
    /* Handle signed data */
    if(temp & 0x0800) {
        temp |= 0xf000;
    }
    instance->temp = (float)temp / 10.0;

    instance->btn = WS_NO_BTN;
    instance->humidity = WS_NO_HUMIDITY;
}

void ws_protocol_decoder_solight_te44_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;

    switch(instance->decoder.parser_step) {
    case SolightTE44DecoderStepReset:
        if((!level) && duration >= ws_protocol_solight_te44_const.te_long) {
            instance->decoder.parser_step = SolightTE44DecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case SolightTE44DecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = SolightTE44DecoderStepCheckDuration;
        } else {
            instance->decoder.parser_step = SolightTE44DecoderStepReset;
        }
        break;

    case SolightTE44DecoderStepCheckDuration:
        if(!level) {
            if(DURATION_DIFF(duration, ws_protocol_solight_te44_const.te_short) <
               ws_protocol_solight_te44_const.te_delta) {
                if(instance->decoder.decode_count_bit ==
                       ws_protocol_solight_te44_const.min_count_bit_for_found &&
                   ws_protocol_solight_te44_check(instance)) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    ws_protocol_solight_te44_extract_data(&instance->generic);

                    if(instance->base.callback) {
                        instance->base.callback(&instance->base, instance->base.context);
                    }
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = SolightTE44DecoderStepReset;
            } else if(
                DURATION_DIFF(instance->decoder.te_last, ws_protocol_solight_te44_const.te_short) <
                ws_protocol_solight_te44_const.te_delta) {
                if(DURATION_DIFF(duration, ws_protocol_solight_te44_const.te_short * 2) <
                   ws_protocol_solight_te44_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                    instance->decoder.parser_step = SolightTE44DecoderStepSaveDuration;
                } else if(
                    DURATION_DIFF(duration, ws_protocol_solight_te44_const.te_short * 4) <
                    ws_protocol_solight_te44_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                    instance->decoder.parser_step = SolightTE44DecoderStepSaveDuration;
                } else
                    instance->decoder.parser_step = SolightTE44DecoderStepReset;
            } else
                instance->decoder.parser_step = SolightTE44DecoderStepReset;
        } else
            instance->decoder.parser_step = SolightTE44DecoderStepReset;
        break;
    }
}

uint32_t ws_protocol_decoder_solight_te44_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_solight_te44_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    ws_protocol_decoder_solight_te44_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    return ws_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        ws_protocol_solight_te44_const.min_count_bit_for_found);
}

void ws_protocol_decoder_solight_te44_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderSolightTE44* instance = context;
    ws_block_generic_get_string(&instance->generic, output);
}
