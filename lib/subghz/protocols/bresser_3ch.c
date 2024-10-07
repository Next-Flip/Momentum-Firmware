#include "bresser_3ch.h"
#include "furi/core/log.h"
#define TAG "WSProtocolBresser3ch"

/*
 * Bresser sensor protocol in versions V1 and V0
 *
 * ----------------------------------------------------------------------------
 * Bresser-3CH V1 format
 * ----------------------------------------------------------------------------
 * 
 * Help:
 * https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_3ch.c
 *
 * The protocol is for the wireless Temperature/Humidity sensor
 * - Bresser Thermo-/Hygro-Sensor 3CH
 * - also works for Renkforce DM-7511
 *
 * The sensor sends 15 identical packages of 40 bits each ~60s.
 * The bits are PWM modulated with On Off Keying.
 *
 * A short pulse of 250 us followed by a 500 us gap is a 0 bit,
 * a long pulse of 500 us followed by a 250 us gap is a 1 bit,
 * there is a sync preamble of pulse, gap, 750 us each, repeated 4 times.
 * Actual received and demodulated timings might be 2% shorter.
 * 
 * The data is grouped in 5 bytes / 10 nibbles
 * 
 *     [id] [id] [flags] [temp] [temp] [temp] [humi] [humi] [chk] [chk]
 * 
 * - id is an 8 bit random id that is generated when the sensor starts
 * - flags are 4 bits battery low indicator, test button press and channel
 * - temp is 12 bit unsigned fahrenheit offset by 90 and scaled by 10
 * - humi is 8 bit relative humidity percentage
 * - chk is the sum of the four data bytes 
 * 
 * ----------------------------------------------------------------------------
 * Bresser-3CH V0 format
 * ----------------------------------------------------------------------------
 * 
 * Typically gets sent before V1: same weather station parameters, but in another format.
 * Probably for older weather stations, as it seems simpler and without a CRC check. No rtl_433 driver yet. 
 *
 * Encoding, PPM modulated with 0 and 1 swapped:
 * - Data sync begins with     475 -3975
 * - A zero is encoded         475 -990
 * - A one is encoded          475 -1980
 *
 * Very much deduced data structure:
 *
 * - 8 repetitions of the same 52 bit payload, a sync between each repetition
 * - 52 bit payload format: IIII IIII CCBB TTTT TTTT TTTT 1111 HHHH HHHH 0000 0000 0000 0000
 *                          ^         ^    ^              ^    ^         ^
 *                          |         |    |              |    |         |
 *        8 bit sensor ID -/          |    |              |    |         |
 *        2 bit channel, bat and btn /     |              |    |         |
 *        12 bit signed int temp(C/10) ---/              /     |         |
 *        separator ------------------------------------/     /         /
 *        8 bit relative humidity, percentage ---------------/         /
 *        16 bit zero tail to end (omitted in the saved data) --------/
 *
 * @m7i-org - because there's more stuff screaming in the ether than you might think
 *
 */

#define BRESSER_V0_DATA          36
#define BRESSER_V0_DATA_AND_TAIL 52
#define BRESSER_V1_DATA          40

static const SubGhzBlockConst ws_protocol_bresser_3ch_v0_const = {
    .te_short = 475,
    .te_long = 3900,
    .te_delta = 150,
};

static const SubGhzBlockConst ws_protocol_bresser_3ch_v1_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 150,
};

struct WSProtocolDecoderBresser3ch {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
};

struct WSProtocolEncoderBresser3ch {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

const SubGhzProtocolDecoder ws_protocol_bresser_3ch_decoder = {
    .alloc = ws_protocol_decoder_bresser_3ch_alloc,
    .free = ws_protocol_decoder_bresser_3ch_free,

    .feed = ws_protocol_decoder_bresser_3ch_feed,
    .reset = ws_protocol_decoder_bresser_3ch_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = ws_protocol_decoder_bresser_3ch_get_hash_data,
    .serialize = ws_protocol_decoder_bresser_3ch_serialize,
    .deserialize = ws_protocol_decoder_bresser_3ch_deserialize,
    .get_string = ws_protocol_decoder_bresser_3ch_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder ws_protocol_bresser_3ch_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_bresser_3ch = {
    .name = WS_PROTOCOL_BRESSER_3CH_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_868 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save,
    .filter = SubGhzProtocolFilter_Weather,
    .decoder = &ws_protocol_bresser_3ch_decoder,
    .encoder = &ws_protocol_bresser_3ch_encoder,
};

typedef enum {
    Bresser3chDecoderStepReset = 0,
    Bresser3chDecoderStepV0SaveDuration,
    Bresser3chDecoderStepV0CheckDuration,
    Bresser3chDecoderStepV0TailCheckDuration,
    Bresser3chDecoderStepV1PreambleDn,
    Bresser3chDecoderStepV1PreambleUp,
    Bresser3chDecoderStepV1SaveDuration,
    Bresser3chDecoderStepV1CheckDuration,
} Bresser3chDecoderStepV1;

void* ws_protocol_decoder_bresser_3ch_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    WSProtocolDecoderBresser3ch* instance = malloc(sizeof(WSProtocolDecoderBresser3ch));
    instance->base.protocol = &ws_protocol_bresser_3ch;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void ws_protocol_decoder_bresser_3ch_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;
    free(instance);
}

void ws_protocol_decoder_bresser_3ch_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;
    instance->decoder.parser_step = Bresser3chDecoderStepReset;
}

static bool ws_protocol_bresser_3ch_check_v0(WSProtocolDecoderBresser3ch* instance) {
    if(!instance->decoder.decode_data) return false;

    // No CRC, so better sanity checks here

    if(((instance->decoder.decode_data >> 8) & 0x0f) != 0x0f) return false; // separator not 0xf
    if(((instance->decoder.decode_data >> 28) & 0xff) == 0xff) return false; // ID only ones?
    if(((instance->decoder.decode_data >> 28) & 0xff) == 0x00) return false; // ID only zeroes?
    if(((instance->decoder.decode_data >> 25) & 0x0f) == 0x0f) return false; // flags only ones?
    if(((instance->decoder.decode_data >> 25) & 0x0f) == 0x00) return false; // flags only zeroes?
    if(((instance->decoder.decode_data >> 12) & 0x0fff) == 0x0fff)
        return false; // temperature maxed out?
    if((instance->decoder.decode_data & 0xff) < 20)
        return false; // humidity percentage less than 20?
    if((instance->decoder.decode_data & 0xff) > 95)
        return false; // humidity percentage more than 95?

    return true;
}

static bool ws_protocol_bresser_3ch_check_v1(WSProtocolDecoderBresser3ch* instance) {
    if(!instance->decoder.decode_data) return false;

    uint8_t sum = (((instance->decoder.decode_data >> 32) & 0xff) +
                   ((instance->decoder.decode_data >> 24) & 0xff) +
                   ((instance->decoder.decode_data >> 16) & 0xff) +
                   ((instance->decoder.decode_data >> 8) & 0xff)) &
                  0xff;

    return (instance->decoder.decode_data & 0xff) == sum;
}

/**
 * Analysis of received data for V0
 * @param instance Pointer to a WSBlockGeneric* instance
 */
static void ws_protocol_bresser_3ch_extract_data_v0(WSBlockGeneric* instance) {
    instance->id = (instance->data >> 28) & 0xff;
    instance->channel = ((instance->data >> 27) & 0x01) | (((instance->data >> 26) & 0x01) << 1);
    instance->btn = ((instance->data >> 25) & 0x1);
    instance->battery_low = ((instance->data >> 24) & 0x1);

    int16_t temp = (instance->data >> 12) & 0x0fff;
    /* Handle signed data */
    if(temp & 0x0800) {
        temp |= 0xf000;
    }
    instance->temp = (float)temp / 10.0;

    instance->humidity = instance->data & 0xff;
}

/**
 * Analysis of received data for V1
 * @param instance Pointer to a WSBlockGeneric* instance
 */
static void ws_protocol_bresser_3ch_extract_data_v1(WSBlockGeneric* instance) {
    instance->id = (instance->data >> 32) & 0xff;
    instance->battery_low = ((instance->data >> 31) & 0x1);
    instance->btn = (instance->data >> 30) & 0x1;
    instance->channel = (instance->data >> 28) & 0x3;

    int16_t temp = (instance->data >> 16) & 0xfff;
    instance->temp = locale_fahrenheit_to_celsius((float)(temp - 900) / 10.0);

    instance->humidity = (instance->data >> 8) & 0xff;
}

void ws_protocol_decoder_bresser_3ch_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;

    switch(instance->decoder.parser_step) {
    case Bresser3chDecoderStepReset:
        if(level && DURATION_DIFF(duration, ws_protocol_bresser_3ch_v1_const.te_short * 3) <
                        ws_protocol_bresser_3ch_v1_const.te_delta) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = Bresser3chDecoderStepV1PreambleDn;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        } else if((!level) && duration >= ws_protocol_bresser_3ch_v0_const.te_long) {
            instance->decoder.parser_step = Bresser3chDecoderStepV0SaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case Bresser3chDecoderStepV0SaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            if(instance->decoder.decode_count_bit < BRESSER_V0_DATA) {
                instance->decoder.parser_step = Bresser3chDecoderStepV0CheckDuration;
            } else {
                instance->decoder.parser_step = Bresser3chDecoderStepV0TailCheckDuration;
            }
        } else {
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        }
        break;

    case Bresser3chDecoderStepV0CheckDuration:
        if(!level) {
            if(DURATION_DIFF(instance->decoder.te_last, ws_protocol_bresser_3ch_v0_const.te_short) <
               ws_protocol_bresser_3ch_v0_const.te_delta) {
                if(DURATION_DIFF(duration, ws_protocol_bresser_3ch_v0_const.te_short * 2) <
                   ws_protocol_bresser_3ch_v0_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                    instance->decoder.parser_step = Bresser3chDecoderStepV0SaveDuration;
                } else if(
                    DURATION_DIFF(duration, ws_protocol_bresser_3ch_v0_const.te_short * 4) <
                    ws_protocol_bresser_3ch_v0_const.te_delta) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                    instance->decoder.parser_step = Bresser3chDecoderStepV0SaveDuration;
                } else
                    instance->decoder.parser_step = Bresser3chDecoderStepReset;
            } else
                instance->decoder.parser_step = Bresser3chDecoderStepReset;
        } else
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        break;

    case Bresser3chDecoderStepV0TailCheckDuration:
        if(!level) {
            if(duration >= ws_protocol_bresser_3ch_v0_const.te_long) {
                if(instance->decoder.decode_count_bit == BRESSER_V0_DATA_AND_TAIL &&
                   ws_protocol_bresser_3ch_check_v0(instance)) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->decoder.decode_count_bit = BRESSER_V0_DATA;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    ws_protocol_bresser_3ch_extract_data_v0(&instance->generic);

                    if(instance->base.callback) {
                        instance->base.callback(&instance->base, instance->base.context);
                    }
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = Bresser3chDecoderStepReset;
            } else if(
                instance->decoder.decode_count_bit < BRESSER_V0_DATA_AND_TAIL &&
                DURATION_DIFF(
                    instance->decoder.te_last, ws_protocol_bresser_3ch_v0_const.te_short) <
                    ws_protocol_bresser_3ch_v0_const.te_delta &&
                DURATION_DIFF(duration, ws_protocol_bresser_3ch_v0_const.te_short * 2) <
                    ws_protocol_bresser_3ch_v0_const.te_delta) {
                instance->decoder.decode_count_bit++;
                instance->decoder.parser_step = Bresser3chDecoderStepV0SaveDuration;
            } else
                instance->decoder.parser_step = Bresser3chDecoderStepReset;
        } else
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        break;

    case Bresser3chDecoderStepV1PreambleDn:
        if((!level) && DURATION_DIFF(duration, ws_protocol_bresser_3ch_v1_const.te_short * 3) <
                           ws_protocol_bresser_3ch_v1_const.te_delta) {
            if(DURATION_DIFF(
                   instance->decoder.te_last, ws_protocol_bresser_3ch_v1_const.te_short * 12) <
               ws_protocol_bresser_3ch_v1_const.te_delta * 2) {
                // End of sync after 4*750 (12*250) high values, start reading the message
                instance->decoder.parser_step = Bresser3chDecoderStepV1SaveDuration;
            } else {
                instance->decoder.parser_step = Bresser3chDecoderStepV1PreambleUp;
            }
        } else {
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        }
        break;

    case Bresser3chDecoderStepV1PreambleUp:
        if(level && DURATION_DIFF(duration, ws_protocol_bresser_3ch_v1_const.te_short * 3) <
                        ws_protocol_bresser_3ch_v1_const.te_delta) {
            instance->decoder.te_last = instance->decoder.te_last + duration;
            instance->decoder.parser_step = Bresser3chDecoderStepV1PreambleDn;
        } else {
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        }
        break;

    case Bresser3chDecoderStepV1SaveDuration:
        if(instance->decoder.decode_count_bit == BRESSER_V1_DATA) {
            if(ws_protocol_bresser_3ch_check_v1(instance)) {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                ws_protocol_bresser_3ch_extract_data_v1(&instance->generic);

                if(instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }
            }
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        } else if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = Bresser3chDecoderStepV1CheckDuration;
        } else {
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        }
        break;

    case Bresser3chDecoderStepV1CheckDuration:
        if(!level) {
            if(DURATION_DIFF(instance->decoder.te_last, ws_protocol_bresser_3ch_v1_const.te_short) <
                   ws_protocol_bresser_3ch_v1_const.te_delta &&
               DURATION_DIFF(duration, ws_protocol_bresser_3ch_v1_const.te_long) <
                   ws_protocol_bresser_3ch_v1_const.te_delta) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = Bresser3chDecoderStepV1SaveDuration;
            } else if(
                DURATION_DIFF(instance->decoder.te_last, ws_protocol_bresser_3ch_v1_const.te_long) <
                    ws_protocol_bresser_3ch_v1_const.te_delta &&
                DURATION_DIFF(duration, ws_protocol_bresser_3ch_v1_const.te_short) <
                    ws_protocol_bresser_3ch_v1_const.te_delta) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = Bresser3chDecoderStepV1SaveDuration;
            } else
                instance->decoder.parser_step = Bresser3chDecoderStepReset;
        } else
            instance->decoder.parser_step = Bresser3chDecoderStepReset;
        break;
    }
}

uint32_t ws_protocol_decoder_bresser_3ch_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_bresser_3ch_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;
    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    ws_protocol_decoder_bresser_3ch_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;

    SubGhzProtocolStatus ret = ws_block_generic_deserialize(&instance->generic, flipper_format);

    if(ret == SubGhzProtocolStatusOk) {
        if(instance->generic.data_count_bit != BRESSER_V0_DATA &&
           instance->generic.data_count_bit != BRESSER_V1_DATA) {
            FURI_LOG_D(TAG, "Wrong number of bits in key for Bresser-3CH V0 or V1 packet format");
            ret = SubGhzProtocolStatusErrorValueBitCount;
        }
    }

    return ret;
}

void ws_protocol_decoder_bresser_3ch_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderBresser3ch* instance = context;
    ws_block_generic_get_string(&instance->generic, output);
}
