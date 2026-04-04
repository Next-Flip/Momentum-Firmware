/**
 * @file secplus_v2_encoder.c
 * @brief Security+ 2.0 SubGHz encoder — SD-card plugin FAL.
 *
 * Compiled as FlipperAppType.PLUGIN and deployed to:
 *   /ext/apps_data/subghz/plugins/subghz_encoder_secplus_v2.fal
 *
 * Self-contained: encode-path helpers are included directly so the main
 * firmware only needs to link the decoder half of secplus_v2.c.
 */

#include <flipper_application/flipper_application.h>
#include <lib/subghz/protocols/subghz_encoder_plugin.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <lib/subghz/blocks/custom_btn_i.h>
#include <lib/toolbox/manchester_encoder.h>
#include <furi.h>
#include <furi_hal.h>

#define TAG "SecPlusV2Enc"

#define SECPLUS_V2_HEADER   0x3C0000000000ULL
#define SECPLUS_V2_PACKET_1 0x000000000000ULL
#define SECPLUS_V2_PACKET_2 0x010000000000ULL

static const SubGhzBlockConst sp2_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 110,
    .min_count_bit_for_found = 62,
};

typedef struct {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
    uint64_t secplus_packet_1;
} SP2Ctx;

static bool sp2_mix_invert(uint8_t inv, uint16_t p[]) {
    switch(inv) {
    case 0x00:
        p[0] = ~p[0] & 0x3FF;
        p[1] = ~p[1] & 0x3FF;
        break;
    case 0x01:
        p[1] = ~p[1] & 0x3FF;
        break;
    case 0x02:
        p[2] = ~p[2] & 0x3FF;
        break;
    case 0x04:
        p[0] = ~p[0] & 0x3FF;
        p[1] = ~p[1] & 0x3FF;
        p[2] = ~p[2] & 0x3FF;
        break;
    case 0x05:
    case 0x0a:
        p[0] = ~p[0] & 0x3FF;
        p[2] = ~p[2] & 0x3FF;
        break;
    case 0x06:
        p[1] = ~p[1] & 0x3FF;
        p[2] = ~p[2] & 0x3FF;
        break;
    case 0x08:
        p[0] = ~p[0] & 0x3FF;
        break;
    case 0x09:
        break;
    default:
        FURI_LOG_E(TAG, "Invert FAIL");
        return false;
    }
    return true;
}

static bool sp2_order_decode(uint8_t ord, uint16_t p[]) {
    uint16_t a = p[0], b = p[1], c = p[2];
    switch(ord) {
    case 0x06:
    case 0x09:
        p[2] = a;
        p[0] = c;
        break;
    case 0x08:
    case 0x04:
        p[1] = a;
        p[2] = b;
        p[0] = c;
        break;
    case 0x01:
        p[2] = a;
        p[0] = b;
        p[1] = c;
        break;
    case 0x00:
        p[2] = b;
        p[1] = c;
        break;
    case 0x05:
        p[1] = a;
        p[0] = b;
        break;
    case 0x02:
    case 0x0A:
        break;
    default:
        FURI_LOG_E(TAG, "Order FAIL");
        return false;
    }
    return true;
}

static bool sp2_order_encode(uint8_t ord, uint16_t p[]) {
    uint16_t a, b, c;
    switch(ord) {
    case 0x06:
    case 0x09:
        a = p[2];
        b = p[1];
        c = p[0];
        break;
    case 0x08:
    case 0x04:
        a = p[1];
        b = p[2];
        c = p[0];
        break;
    case 0x01:
        a = p[2];
        b = p[0];
        c = p[1];
        break;
    case 0x00:
        a = p[0];
        b = p[2];
        c = p[1];
        break;
    case 0x05:
        a = p[1];
        b = p[0];
        c = p[2];
        break;
    case 0x02:
    case 0x0A:
        a = p[0];
        b = p[1];
        c = p[2];
        break;
    default:
        FURI_LOG_E(TAG, "Order FAIL");
        return false;
    }
    p[0] = a;
    p[1] = b;
    p[2] = c;
    return true;
}

static bool sp2_decode_half(uint64_t data, uint8_t roll[], uint32_t* fixed) {
    uint8_t ord = (data >> 34) & 0x0f, inv = (data >> 30) & 0x0f;
    uint16_t p[3] = {0};
    for(int i = 29; i >= 0; i -= 3) {
        p[0] = p[0] << 1 | bit_read(data, i);
        p[1] = p[1] << 1 | bit_read(data, i - 1);
        p[2] = p[2] << 1 | bit_read(data, i - 2);
    }
    if(!sp2_mix_invert(inv, p) || !sp2_order_decode(ord, p)) return false;
    data = ord << 4 | inv;
    int k = 0;
    for(int i = 6; i >= 0; i -= 2) {
        roll[k] = (data >> i) & 3;
        if(roll[k++] == 3) return false;
    }
    for(int i = 8; i >= 0; i -= 2) {
        roll[k] = (p[2] >> i) & 3;
        if(roll[k++] == 3) return false;
    }
    fixed[0] = p[0] << 10 | p[1];
    return true;
}

static uint64_t sp2_encode_half(uint8_t roll[], uint32_t fixed) {
    uint64_t data = 0;
    uint16_t p[3] = {(fixed >> 10) & 0x3FF, fixed & 0x3FF, 0};
    uint8_t ord = roll[0] << 2 | roll[1], inv = roll[2] << 2 | roll[3];
    p[2] = (uint16_t)roll[4] << 8 | roll[5] << 6 | roll[6] << 4 | roll[7] << 2 | roll[8];
    if(!sp2_order_encode(ord, p) || !sp2_mix_invert(inv, p)) return 0;
    for(int i = 0; i < 10; i++) {
        data <<= 3;
        data |= bit_read(p[0], 9 - i) << 2 | bit_read(p[1], 9 - i) << 1 | bit_read(p[2], 9 - i);
    }
    data |= ((uint64_t)ord) << 34 | ((uint64_t)inv) << 30;
    return data;
}

static void sp2_remote_controller(SubGhzBlockGeneric* g, uint64_t pkt1) {
    uint32_t f1[1], f2[1];
    uint8_t r1[9] = {0}, r2[9] = {0}, d[18] = {0};
    if(sp2_decode_half(pkt1, r1, f1) && sp2_decode_half(g->data, r2, f2)) {
        d[0] = r2[8];
        d[1] = r1[8];
        d[2] = r2[4];
        d[3] = r2[5];
        d[4] = r2[6];
        d[5] = r2[7];
        d[6] = r1[4];
        d[7] = r1[5];
        d[8] = r1[6];
        d[9] = r1[7];
        d[10] = r2[0];
        d[11] = r2[1];
        d[12] = r2[2];
        d[13] = r2[3];
        d[14] = r1[0];
        d[15] = r1[1];
        d[16] = r1[2];
        d[17] = r1[3];
        uint32_t rolling = 0;
        for(int i = 0; i < 18; i++)
            rolling = (rolling * 3) + d[i];
        if(rolling >= 0x10000000) {
            g->cnt = 0;
            g->btn = 0;
            g->serial = 0;
        } else {
            g->cnt = subghz_protocol_blocks_reverse_key(rolling, 28);
            g->btn = f1[0] >> 12;
            g->serial = f1[0] << 20 | f2[0];
        }
    } else {
        g->cnt = 0;
        g->btn = 0;
        g->serial = 0;
    }
}

static uint8_t sp2_get_btn(void) {
    uint8_t id = subghz_custom_btn_get(), orig = subghz_custom_btn_get_original(), btn = orig;
    if(id == SUBGHZ_CUSTOM_BTN_OK && orig) return orig;
    if(id == SUBGHZ_CUSTOM_BTN_UP) {
        switch(orig) {
        case 0x68:
            return 0x80;
        case 0x80:
            return 0x68;
        default:
            return 0x80;
        }
    }
    if(id == SUBGHZ_CUSTOM_BTN_DOWN) {
        switch(orig) {
        case 0x81:
            return 0x68;
        default:
            return 0x81;
        }
    }
    if(id == SUBGHZ_CUSTOM_BTN_LEFT) {
        switch(orig) {
        case 0xE2:
            return 0x68;
        default:
            return 0xE2;
        }
    }
    if(id == SUBGHZ_CUSTOM_BTN_RIGHT) {
        switch(orig) {
        case 0x78:
            return 0x68;
        default:
            return 0x78;
        }
    }
    return btn;
}

static void sp2_encode(SP2Ctx* ctx) {
    ctx->generic.btn = sp2_get_btn();
    if(subghz_block_generic_global_button_override_get(&ctx->generic.btn))
        FURI_LOG_D(TAG, "Button->0x%X", ctx->generic.btn);
    uint32_t f1[1] = {ctx->generic.btn << 12 | ctx->generic.serial >> 20};
    uint32_t f2[1] = {ctx->generic.serial & 0xFFFFF};
    uint8_t d[18] = {0}, r1[9] = {0}, r2[9] = {0};
    if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {
        if(!subghz_block_generic_global_counter_override_get(&ctx->generic.cnt)) {
            if((ctx->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) > 0xFFFFFFF)
                ctx->generic.cnt = 0xE500000;
            else
                ctx->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
        }
        if(ctx->generic.cnt < 0xE500000) ctx->generic.cnt = 0xE500000;
    } else {
        if((ctx->generic.cnt + 1) > 0xFFFFFFF)
            ctx->generic.cnt = 0xE500000;
        else if(ctx->generic.cnt >= 0xE500000 && ctx->generic.cnt != 0xFFFFFFE)
            ctx->generic.cnt = 0xFFFFFFE;
        else
            ctx->generic.cnt++;
    }
    uint32_t rolling = subghz_protocol_blocks_reverse_key(ctx->generic.cnt, 28);
    for(int8_t i = 17; i > -1; i--) {
        d[i] = rolling % 3;
        rolling /= 3;
    }
    r2[8] = d[0];
    r1[8] = d[1];
    r2[4] = d[2];
    r2[5] = d[3];
    r2[6] = d[4];
    r2[7] = d[5];
    r1[4] = d[6];
    r1[5] = d[7];
    r1[6] = d[8];
    r1[7] = d[9];
    r2[0] = d[10];
    r2[1] = d[11];
    r2[2] = d[12];
    r2[3] = d[13];
    r1[0] = d[14];
    r1[1] = d[15];
    r1[2] = d[16];
    r1[3] = d[17];
    ctx->secplus_packet_1 = SECPLUS_V2_HEADER | SECPLUS_V2_PACKET_1 | sp2_encode_half(r1, f1[0]);
    ctx->generic.data = SECPLUS_V2_HEADER | SECPLUS_V2_PACKET_2 | sp2_encode_half(r2, f2[0]);
}

static LevelDuration sp2_ld(ManchesterEncoderResult r) {
    switch(r) {
    case ManchesterEncoderResultShortLow:
        return level_duration_make(false, sp2_const.te_short);
    case ManchesterEncoderResultLongLow:
        return level_duration_make(false, sp2_const.te_long);
    case ManchesterEncoderResultLongHigh:
        return level_duration_make(true, sp2_const.te_long);
    case ManchesterEncoderResultShortHigh:
        return level_duration_make(true, sp2_const.te_short);
    default:
        furi_crash("SubGhz: Manchester result incorrect.");
        return level_duration_reset();
    }
}

static void sp2_get_upload(SP2Ctx* ctx) {
    size_t idx = 0;
    ManchesterEncoderState st;
    ManchesterEncoderResult r;
    manchester_encoder_reset(&st);
    for(uint8_t i = ctx->generic.data_count_bit; i > 0; i--) {
        if(!manchester_encoder_advance(&st, bit_read(ctx->secplus_packet_1, i - 1), &r)) {
            ctx->encoder.upload[idx++] = sp2_ld(r);
            manchester_encoder_advance(&st, bit_read(ctx->secplus_packet_1, i - 1), &r);
        }
        ctx->encoder.upload[idx++] = sp2_ld(r);
    }
    ctx->encoder.upload[idx] = sp2_ld(manchester_encoder_finish(&st));
    if(level_duration_get_level(ctx->encoder.upload[idx])) idx++;
    ctx->encoder.upload[idx++] = level_duration_make(false, (uint32_t)sp2_const.te_long * 136);
    manchester_encoder_reset(&st);
    for(uint8_t i = ctx->generic.data_count_bit; i > 0; i--) {
        if(!manchester_encoder_advance(&st, bit_read(ctx->generic.data, i - 1), &r)) {
            ctx->encoder.upload[idx++] = sp2_ld(r);
            manchester_encoder_advance(&st, bit_read(ctx->generic.data, i - 1), &r);
        }
        ctx->encoder.upload[idx++] = sp2_ld(r);
    }
    ctx->encoder.upload[idx] = sp2_ld(manchester_encoder_finish(&st));
    if(level_duration_get_level(ctx->encoder.upload[idx])) idx++;
    ctx->encoder.upload[idx++] = level_duration_make(false, (uint32_t)sp2_const.te_long * 136);
    ctx->encoder.size_upload = idx;
}

static SubGhzProtocolEncoderBase* sp2_alloc(SubGhzEnvironment* env) {
    UNUSED(env);
    SP2Ctx* ctx = malloc(sizeof(SP2Ctx));
    ctx->base.protocol = NULL;
    ctx->generic.protocol_name = "Security+ 2.0";
    ctx->encoder.repeat = 3;
    ctx->encoder.size_upload = 256;
    ctx->encoder.upload = malloc(ctx->encoder.size_upload * sizeof(LevelDuration));
    ctx->encoder.is_running = false;
    return (SubGhzProtocolEncoderBase*)ctx;
}

static void sp2_free(void* ctx_) {
    SP2Ctx* ctx = ctx_;
    free(ctx->encoder.upload);
    free(ctx);
}

static SubGhzProtocolStatus sp2_deserialize(void* ctx_, FlipperFormat* ff) {
    SP2Ctx* ctx = ctx_;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &ctx->generic, ff, sp2_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) break;
        uint8_t kd[sizeof(uint64_t)] = {0};
        if(!flipper_format_read_hex(ff, "Secplus_packet_1", kd, sizeof(uint64_t))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        for(uint8_t i = 0; i < sizeof(uint64_t); i++)
            ctx->secplus_packet_1 = ctx->secplus_packet_1 << 8 | kd[i];
        sp2_remote_controller(&ctx->generic, ctx->secplus_packet_1);
        sp2_encode(ctx);
        flipper_format_read_uint32(ff, "Repeat", (uint32_t*)&ctx->encoder.repeat, 1);
        sp2_get_upload(ctx);
        for(size_t i = 0; i < sizeof(uint64_t); i++)
            kd[sizeof(uint64_t) - i - 1] = (ctx->generic.data >> (i * 8)) & 0xFF;
        if(!flipper_format_update_hex(ff, "Key", kd, sizeof(uint64_t))) {
            ret = SubGhzProtocolStatusErrorParserKey;
            break;
        }
        for(size_t i = 0; i < sizeof(uint64_t); i++)
            kd[sizeof(uint64_t) - i - 1] = (ctx->secplus_packet_1 >> (i * 8)) & 0xFF;
        if(!flipper_format_update_hex(ff, "Secplus_packet_1", kd, sizeof(uint64_t))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        ctx->encoder.front = 0;
        ctx->encoder.is_running = true;
    } while(false);
    return ret;
}

static void sp2_stop(void* ctx_) {
    SP2Ctx* ctx = ctx_;
    ctx->encoder.is_running = false;
    ctx->encoder.front = 0;
}

static LevelDuration sp2_yield(void* ctx_) {
    SP2Ctx* ctx = ctx_;
    if(ctx->encoder.repeat == 0 || !ctx->encoder.is_running) {
        ctx->encoder.is_running = false;
        return level_duration_reset();
    }
    LevelDuration ret = ctx->encoder.upload[ctx->encoder.front];
    if(++ctx->encoder.front == ctx->encoder.size_upload) {
        if(!subghz_block_generic_global.endless_tx) ctx->encoder.repeat--;
        ctx->encoder.front = 0;
    }
    return ret;
}

static const SubGhzEncoderPlugin sp2_plugin = {
    .alloc = sp2_alloc,
    .free = sp2_free,
    .deserialize = sp2_deserialize,
    .stop = sp2_stop,
    .yield = sp2_yield,
};

static const FlipperAppPluginDescriptor sp2_descriptor = {
    .appid = SUBGHZ_ENCODER_PLUGIN_APP_ID,
    .ep_api_version = SUBGHZ_ENCODER_PLUGIN_API_VERSION,
    .entry_point = &sp2_plugin,
};

const FlipperAppPluginDescriptor* secplus_v2_encoder_plugin_ep(void) {
    return &sp2_descriptor;
}
