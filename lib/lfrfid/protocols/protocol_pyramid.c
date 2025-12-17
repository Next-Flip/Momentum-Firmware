
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
    (PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE + PYRAMID_PREAMBLE_SIZE)           // 19 bytes
#define PYRAMID_ENCODED_BIT_SIZE  ((PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE) * 8)
#define PYRAMID_DECODED_DATA_SIZE (4)

// --- Assumed Wiegand mapping for 81-bit (adjust if needed) ---
#define PYR81_FC_LEN_BITS       8
#define PYR81_CARD_LEN_BITS     16
#define PYR81_FC_OFFSET_FROM_J  1   // FC starts immediately after the start bit
#define PYR81_CARD_OFFSET_FROM_J (PYR81_FC_OFFSET_FROM_J + PYR81_FC_LEN_BITS)

typedef struct {
    FSKDemod* fsk_demod;
} ProtocolPyramidDecoder;

typedef struct {
    FSKOsc* fsk_osc;
    uint8_t encoded_index;
    uint32_t pulse;
} ProtocolPyramidEncoder;

typedef struct {
    ProtocolPyramidDecoder decoder;
    ProtocolPyramidEncoder encoder;
    uint8_t encoded_data[PYRAMID_ENCODED_DATA_SIZE];
    uint8_t data[PYRAMID_DECODED_DATA_SIZE];

    // Debug: snapshot full frame
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
    protocol->encoder.fsk_osc  = fsk_osc_alloc(8, 10, 50);
    memset(protocol->raw_dbg, 0, sizeof(protocol->raw_dbg));
    protocol->raw_dbg_len = 0;
    return protocol;
}

void protocol_pyramid_free(ProtocolPyramid* protocol) {
    fsk_demod_free(protocol->decoder.fsk_demod);
    fsk_osc_free(protocol->encoder.fsk_osc);
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
}

// Non-mutating validation + preamble fix
static bool protocol_pyramid_can_be_decoded(const uint8_t* data_in) {
    uint8_t tmp[PYRAMID_ENCODED_DATA_SIZE];
    memcpy(tmp, data_in, sizeof(tmp));

    // Front preamble 00 01 01
    if(bit_lib_get_bits_16(tmp, 0, 16) != 0x0001 ||
       bit_lib_get_bits(tmp, 16, 8)   != 0x01) {
        return false;
    }

    // Trailing preamble bytes 16..18 == 00 01 01
    if(bit_lib_get_bits_16(tmp, 128, 16) != 0x0001) {
        return false;
    }
    // FIX: final byte at bit 144 (not 136)
    if(bit_lib_get_bits(tmp, 144, 8) != 0x01) {
        return false;
    }

    // CRC8 over 13 payload bytes
    const uint8_t checksum = bit_lib_get_bits(tmp, 120, 8);
    uint8_t checksum_data[13] = {0};
    for(uint8_t i = 0; i < 13; i++) {
        checksum_data[i] = bit_lib_get_bits(tmp, 16 + (i * 8), 8);
    }
    const uint8_t calc_checksum = bit_lib_crc8(checksum_data, 13, 0x31, 0x00, true, true, 0x00);
    if(checksum != calc_checksum) return false;

    // Remove parity on local copy
    bit_lib_remove_bit_every_nth(tmp, 8, 15 * 8, 8);

    // Determine start bit and format length
    int j;
    for(j = 0; j < 105; ++j) {
        if(bit_lib_get_bit(tmp, j)) break;
    }
    const uint8_t fmt_len = 105 - j;

    // Accept 26 and 81 for now
    if(!(fmt_len == 26 || fmt_len == 81)) return false;

    return true;
}

static void protocol_pyramid_decode(ProtocolPyramid* protocol) {
    // Work on parity-removed local copy to compute fmt_len and extract fields
    uint8_t tmp[PYRAMID_ENCODED_DATA_SIZE];
    memcpy(tmp, protocol->encoded_data, sizeof(tmp));
    bit_lib_remove_bit_every_nth(tmp, 8, 15 * 8, 8);

    // Find start bit j
    int j;
    for(j = 0; j < 105; ++j) {
        if(bit_lib_get_bit(tmp, j)) break;
    }
    const uint8_t fmt_len = 105 - j;

    // Write format length
    bit_lib_set_bits(protocol->data, 0, fmt_len, 8);

    if(fmt_len == 26) {
        // FC (8 bits)
        bit_lib_copy_bits(protocol->data, 8, 8,  protocol->encoded_data, 73 + 8);
        // Card (16 bits)
        bit_lib_copy_bits(protocol->data, 16, 16, protocol->encoded_data, 81 + 8);

    } else if(fmt_len == 81) {
        // --- Assumed mapping: FC = 8 bits immediately after start bit; Card = next 16 bits ---
        // If your site’s 81-bit variant places FC/Card elsewhere, tweak offsets below.

        // FC
        bit_lib_copy_bits(protocol->data, 8, PYR81_FC_LEN_BITS,  tmp, j + PYR81_FC_OFFSET_FROM_J);

        // Card
        bit_lib_copy_bits(
            protocol->data,
            16,
            PYR81_CARD_LEN_BITS,
            tmp,
            j + PYR81_CARD_OFFSET_FROM_J
        );
    }
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
                // Snapshot full frame for debug
                memcpy(protocol->raw_dbg, protocol->encoded_data, PYRAMID_ENCODED_DATA_SIZE);
                protocol->raw_dbg_len = PYRAMID_ENCODED_DATA_SIZE;

                protocol_pyramid_decode(protocol);
                result = true;
            }
        }
    }
    return result;
}

/* ---------- Parity helpers + Encoder/Writer (unchanged) ---------- */

bool protocol_pyramid_get_parity(const uint8_t* bits, uint8_t type, int length) {
    int x;
    for(x = 0; length > 0; --length)
        x += bit_lib_get_bit(bits, length - 1);
    x %= 2;
    return x ^ type;
}

void protocol_pyramid_add_wiegand_parity(
    uint8_t* target,
    uint8_t target_position,
    uint8_t* source,
    uint8_t length) {
    bit_lib_set_bit(target, target_position, protocol_pyramid_get_parity(source, 0, length / 2));
    bit_lib_copy_bits(target, target_position + 1, length, source, 0);
    bit_lib_set_bit(target, target_position + length + 1,
                    protocol_pyramid_get_parity(source + length / 2, 1, length / 2));
}

static void protocol_pyramid_encode(ProtocolPyramid* protocol) {
    memset(protocol->encoded_data, 0, sizeof(protocol->encoded_data));

    uint8_t pre[16] = {0};
    bit_lib_set_bit(pre, 79, 1); // format start bit

    uint8_t wiegand[3] = {0};
    bit_lib_copy_bits(wiegand, 0, 8,  protocol->data, 8);   // FC
    bit_lib_copy_bits(wiegand, 8, 16, protocol->data, 16);  // Card

    protocol_pyramid_add_wiegand_parity(pre, 80, wiegand, 24);
    bit_lib_add_parity(pre, 8, protocol->encoded_data, 8, 102, 8, 1);

    uint8_t checksum_buffer[13];
    for(uint8_t i = 0; i < 13; i++)
        checksum_buffer[i] = bit_lib_get_bits(protocol->encoded_data, 16 + (i * 8), 8);

    uint8_t crc = bit_lib_crc8(checksum_buffer, 13, 0x31, 0x00, true, true, 0x00);
    bit_lib_set_bits(protocol->encoded_data, 120, crc, 8);
}

bool protocol_pyramid_encoder_start(ProtocolPyramid* protocol) {
    protocol->encoder.encoded_index = 0;
    protocol->encoder.pulse = 0;
    protocol_pyramid_encode(protocol);
    return true;
}

LevelDuration protocol_pyramid_encoder_yield(ProtocolPyramid* protocol) {
    bool level = 0;
    uint32_t duration = 0;

    if(protocol->encoder.pulse == 0) {
        uint8_t bit = bit_lib_get_bit(protocol->encoded_data, protocol->encoder.encoded_index);
        bool advance = fsk_osc_next(protocol->encoder.fsk_osc, bit, &duration);
        if(advance) {
            bit_lib_increment_index(protocol->encoder.encoded_index, PYRAMID_ENCODED_BIT_SIZE);
        }
        duration = duration / 2;
        protocol->encoder.pulse = duration;
        level = true;
    } else {
        duration = protocol->encoder.pulse;
        protocol->encoder.pulse = 0;
        level = false;
    }
    return level_duration_make(level, duration);
}

bool protocol_pyramid_write_data(ProtocolPyramid* protocol, void* data) {
    LFRFIDWriteRequest* request = (LFRFIDWriteRequest*)data;
    bool result = false;

    protocol_pyramid_encode(protocol);
    bit_lib_remove_bit_every_nth(protocol->encoded_data, 8, 15 * 8, 8);
    protocol_pyramid_decode(protocol);

    protocol_pyramid_encoder_start(protocol);

    if(request->write_type == LFRFIDWriteTypeT5577) {
        request->t5577.block[0] = LFRFID_T5577_MODULATION_FSK2a | LFRFID_T5577_BITRATE_RF_50 |
                                  (4 << LFRFID_T5577_MAXBLOCK_SHIFT);
        request->t5577.block[1] = bit_lib_get_bits_32(protocol->encoded_data, 0, 32);
        request->t5577.block[2] = bit_lib_get_bits_32(protocol->encoded_data, 32, 32);
        request->t5577.block[3] = bit_lib_get_bits_32(protocol->encoded_data, 64, 32);
        request->t5577.block[4] = bit_lib_get_bits_32(protocol->encoded_data, 96, 32);
        request->t5577.blocks_to_write = 5;
        result = true;
    }
    return result;
}

/* ---------- Rendering ---------- */

void protocol_pyramid_render_data(ProtocolPyramid* protocol, FuriString* result) {
    uint8_t* decoded_data = protocol->data;
    const uint8_t format_length = decoded_data[0];

    furi_string_printf(result, "Format: %hhu\n", format_length);

    if(format_length == 26) {
        uint8_t facility;
        bit_lib_copy_bits(&facility, 0, 8, decoded_data, 8);

        uint16_t card_id;
        bit_lib_copy_bits((uint8_t*)&card_id, 8, 8, decoded_data, 16);
        bit_lib_copy_bits((uint8_t*)&card_id, 0, 8, decoded_data, 24);
        furi_string_cat_printf(result, "FC: %03hhu; Card: %05hu\n", facility, card_id);

    } else if(format_length == 81) {
        uint8_t facility;
        bit_lib_copy_bits(&facility, 0, 8, decoded_data, 8);

        uint16_t card_id;
        bit_lib_copy_bits((uint8_t*)&card_id, 8, 8, decoded_data, 16);
        bit_lib_copy_bits((uint8_t*)&card_id, 0, 8, decoded_data, 24);

        furi_string_cat_printf(result, "FC: %03hhu; Card: %05hu\n", facility, card_id);

        // Show snapshot to aid offset tuning if needed
        if(protocol->raw_dbg_len) {
            furi_string_cat_printf(result, "Frame(19): ");
            pyramid_append_hex(result, protocol->raw_dbg, protocol->raw_dbg_len);
        }
    } else {
        furi_string_cat_printf(result, "Data: Unknown\n");
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
    .decoder =
        {
            .start = (ProtocolDecoderStart)protocol_pyramid_decoder_start,
            .feed  = (ProtocolDecoderFeed)protocol_pyramid_decoder_feed,
        },
    .encoder =
        {
            .start = (ProtocolEncoderStart)protocol_pyramid_encoder_start,
            .yield = (ProtocolEncoderYield)protocol_pyramid_encoder_yield,
        },
    .render_data       = (ProtocolRenderData)protocol_pyramid_render_data,
    .render_brief_data = (    .render_brief_data = (ProtocolRenderData)protocol_pyramid_render_data,
    .write_data        = (ProtocolWriteData)protocol_pyramid_write_data,
