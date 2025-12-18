#include <stdint.h>
#include <furi.h>
#include <toolbox/protocols/protocol.h>
#include <lfrfid/tools/fsk_demod.h>
#include <lfrfid/tools/fsk_osc.h>
#include "lfrfid_protocols.h"
#include <bit_lib/bit_lib.h>

#define JITTER_TIME (20)
#define MIN_TIME    (64 - JITTER_TIME)
#define MAX_TIME    (80 + JITTER_TIME)

#define PYRAMID_DATA_SIZE     13
#define PYRAMID_PREAMBLE_SIZE 3

#define PYRAMID_ENCODED_DATA_SIZE \
    (PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE + PYRAMID_PREAMBLE_SIZE) // 19 bytes
#define PYRAMID_ENCODED_BIT_SIZE  ((PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE) * 8)
#define PYRAMID_DECODED_DATA_SIZE (4) // [0]=format_len, rest reserved

typedef struct {
    FSKDemod* fsk_demod;
} ProtocolPyramidDecoder;

typedef struct {
    // Present to satisfy the framework; emulation is disabled in this file.
    FSKOsc* fsk_osc;
    uint8_t encoded_index;
    uint32_t pulse;
} ProtocolPyramidEncoder;

typedef struct {
    ProtocolPyramidDecoder decoder;
    ProtocolPyramidEncoder encoder;

    uint8_t encoded_data[PYRAMID_ENCODED_DATA_SIZE];
    uint8_t data[PYRAMID_DECODED_DATA_SIZE];

    // Debug: snapshot full 19-byte frame (front preamble + payload + tail preamble)
    uint8_t raw_dbg[PYRAMID_ENCODED_DATA_SIZE];
    uint8_t raw_dbg_len;
} ProtocolPyramid;

/* ---------- Helpers ---------- */
static void pyramid_append_hex(FuriString* out, const uint8_t* b, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        furi_string_cat_printf(out, "%02X", b[i]);
    }
}

/* ---------- Alloc/free ---------- */
ProtocolPyramid* protocol_pyramid_alloc(void) {
    ProtocolPyramid* protocol = malloc(sizeof(ProtocolPyramid));
    protocol->decoder.fsk_demod = fsk_demod_alloc(MIN_TIME, 6, MAX_TIME, 5);
    protocol->encoder.fsk_osc = fsk_osc_alloc(8, 10, 50);

    memset(protocol->encoded_data, 0, sizeof(protocol->encoded_data));
    memset(protocol->data, 0, sizeof(protocol->data));
    memset(protocol->raw_dbg, 0, sizeof(protocol->raw_dbg));
    protocol->raw_dbg_len = 0;

    protocol->encoder.encoded_index = 0;
    protocol->encoder.pulse = 0;
    return protocol;
}

void protocol_pyramid_free(ProtocolPyramid* protocol) {
    if(!protocol) return;
    if(protocol->decoder.fsk_demod) fsk_demod_free(protocol->decoder.fsk_demod);
    if(protocol->encoder.fsk_osc) fsk_osc_free(protocol->encoder.fsk_osc);
    free(protocol);
}

uint8_t* protocol_pyramid_get_data(ProtocolPyramid* protocol) {
    return protocol->data;
}

/* ---------- Decoder ---------- */
void protocol_pyramid_decoder_start(ProtocolPyramid* protocol) {
    memset(protocol->encoded_data, 0, sizeof(protocol->encoded_data));
    memset(protocol->raw_dbg, 0, sizeof(protocol->raw_dbg));
    protocol->raw_dbg_len = 0;
    memset(protocol->data, 0, sizeof(protocol->data));
}

// Validation (non-mutating)
static bool protocol_pyramid_can_be_decoded(const uint8_t* data_in) {
    uint8_t tmp[PYRAMID_ENCODED_DATA_SIZE];
    memcpy(tmp, data_in, sizeof(tmp));

    // Front preamble: 00 01 01
    if(bit_lib_get_bits_16(tmp, 0, 16) != 0x0001 || bit_lib_get_bits(tmp, 16, 8) != 0x01) {
        return false;
    }

    // Trailing preamble: bytes 16..18 == 00 01 01
    if(bit_lib_get_bits_16(tmp, 128, 16) != 0x0001) {
        return false;
    }
    // IMPORTANT: final byte is at bit 144 (not 136)
    if(bit_lib_get_bits(tmp, 144, 8) != 0x01) {
        return false;
    }

    // CRC8 over 13 payload bytes (bits 16..119), compare with CRC byte at bits 120..127
    const uint8_t checksum = bit_lib_get_bits(tmp, 120, 8);
    uint8_t checksum_data[13] = {0};
    for(uint8_t i = 0; i < 13; i++) {
        checksum_data[i] = bit_lib_get_bits(tmp, 16 + (i * 8), 8);
    }
    const uint8_t calc_checksum = bit_lib_crc8(checksum_data, 13, 0x31, 0x00, true, true, 0x00);
    if(checksum != calc_checksum) return false;

    // Remove parity bits (every 8th after each 7 data bits) on a local copy
    bit_lib_remove_bit_every_nth(tmp, 8, 15 * 8, 8);

    // Determine format length from start bit position (same method you were using)
    int j;
    for(j = 0; j < 105; ++j) {
        if(bit_lib_get_bit(tmp, j)) break;
    }
    const uint8_t fmt_len = 105 - j;

    // Accept formats we know exist in the wild for Pyramid
    return (fmt_len == 26 || fmt_len == 81);
}

static void protocol_pyramid_decode(ProtocolPyramid* protocol) {
    // Make parity-removed local copy only for format length detection
    uint8_t tmp[PYRAMID_ENCODED_DATA_SIZE];
    memcpy(tmp, protocol->encoded_data, sizeof(tmp));
    bit_lib_remove_bit_every_nth(tmp, 8, 15 * 8, 8);

    int j;
    for(j = 0; j < 105; ++j) {
        if(bit_lib_get_bit(tmp, j)) break;
    }
    const uint8_t fmt_len = 105 - j;

    // Store only the format length; do NOT decode FC/Card here (avoids misleading values)
    memset(protocol->data, 0, sizeof(protocol->data));
    bit_lib_set_bits(protocol->data, 0, fmt_len, 8);
}

bool protocol_pyramid_decoder_feed(ProtocolPyramid* protocol, bool level, uint32_t duration) {
    bool value;
    uint32_t count;
    bool result = false;

    fsk_demod_feed(protocol->decoder.fsk_demod, level, duration, &value, &count);
    if(count > 0) {
        for(size_t i = 0; i < count; i++) {
            bit_lib_push_bit(protocol->encoded_data, PYRAMID_ENCODED_DATA_SIZE, value);

            if(protocol_pyramid_can_be_decoded(protocol->encoded_data)) {
                memcpy(protocol->raw_dbg, protocol->encoded_data, PYRAMID_ENCODED_DATA_SIZE);
                protocol->raw_dbg_len = PYRAMID_ENCODED_DATA_SIZE;

                protocol_pyramid_decode(protocol);
                result = true;
            }
        }
    }
    return result;
}

/* ---------- Emulation / Writing DISABLED ---------- */
bool protocol_pyramid_encoder_start(ProtocolPyramid* protocol) {
    (void)protocol;
    return false;
}

LevelDuration protocol_pyramid_encoder_yield(ProtocolPyramid* protocol) {
    (void)protocol;
    return level_duration_make(false, 0);
}

bool protocol_pyramid_write_data(ProtocolPyramid* protocol, void* data) {
    (void)protocol;
    (void)data;
    return false;
}

/* ---------- Render ---------- */
void protocol_pyramid_render_data(ProtocolPyramid* protocol, FuriString* result) {
    const uint8_t format_length = protocol->data[0];

    furi_string_printf(result, "Format: %hhu\n", format_length);

    // Proxmark "Raw:" matches the first 16 bytes (3 preamble + 13 payload)
    if(protocol->raw_dbg_len >= 16) {
        furi_string_cat_printf(result, "Raw(16): ");
        pyramid_append_hex(result, protocol->raw_dbg, 16);
        furi_string_cat_printf(result, "\n");
    }

    // Full captured 19-byte frame (includes trailing 00 01 01)
    if(protocol->raw_dbg_len) {
        furi_string_cat_printf(result, "Frame(19): ");
        pyramid_append_hex(result, protocol->raw_dbg, protocol->raw_dbg_len);
        furi_string_cat_printf(result, "\n");
    }

    // Avoid misleading “FC/Card” for 81-bit
    if(format_length == 81) {
        furi_string_cat_printf(result, "FC/Card: (system-defined / not decoded)\n");
    }
}

const ProtocolBase protocol_pyramid = {
    .name = "Pyramid",
    .manufacturer = "Farpointe",
    .data_size = PYRAMID_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .validate_count = 3,
    .alloc = (ProtocolAlloc)protocol_pyramid_alloc,
    .free = (ProtocolFree)protocol_pyramid_free,
    .get_data = (ProtocolGetData)protocol_pyramid_get_data,
    .decoder = {
        .start = (ProtocolDecoderStart)protocol_pyramid_decoder_start,
        .feed  = (ProtocolDecoderFeed)protocol_pyramid_decoder_feed,
    },
    .encoder = {
        .start = (ProtocolEncoderStart)protocol_pyramid_encoder_start,
        .yield = (ProtocolEncoderYield)protocol_pyramid_encoder_yield,
    },
    .render_data       = (ProtocolRenderData)protocol_pyramid_render_data,
    .render_brief_data = (ProtocolRenderData)protocol_pyramid_render_data,
    .write_data        = (ProtocolWriteData)protocol_pyramid_write_data,
};
