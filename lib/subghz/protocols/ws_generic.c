#include "ws_generic.h"
#include <lib/toolbox/stream/stream.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <float_tools.h>

#define TAG "WSBlockGeneric"

SubGhzProtocolStatus ws_block_generic_serialize(
    WSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(instance);
    SubGhzProtocolStatus res =
        subghz_block_generic_serialize_common(instance->protocol_name, flipper_format, preset);
    if(res != SubGhzProtocolStatusOk) return res;
    res = SubGhzProtocolStatusError;
    do {
        uint32_t temp_data = instance->id;
        if(!flipper_format_write_uint32(flipper_format, "Id", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Id");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        temp_data = instance->data_count_bit;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Bit");
            res = SubGhzProtocolStatusErrorParserBitCount;
            break;
        }

        uint8_t key_data[sizeof(uint64_t)] = {0};
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            key_data[sizeof(uint64_t) - i - 1] = (instance->data >> (i * 8)) & 0xFF;
        }

        if(!flipper_format_write_hex(flipper_format, "Data", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Unable to add Data");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        temp_data = instance->battery_low;
        if(!flipper_format_write_uint32(flipper_format, "Batt", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Battery_low");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        temp_data = instance->humidity;
        if(!flipper_format_write_uint32(flipper_format, "Hum", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Humidity");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        //DATE AGE set
        uint32_t curr_ts = furi_hal_rtc_get_timestamp();

        temp_data = curr_ts;
        if(!flipper_format_write_uint32(flipper_format, "Ts", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add timestamp");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        temp_data = instance->channel;
        if(!flipper_format_write_uint32(flipper_format, "Ch", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Channel");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        temp_data = instance->btn;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp_data, 1)) {
            FURI_LOG_E(TAG, "Unable to add Btn");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        float temp = instance->temp;
        if(!flipper_format_write_float(flipper_format, "Temp", &temp, 1)) {
            FURI_LOG_E(TAG, "Unable to add Temperature");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        res = SubGhzProtocolStatusOk;
    } while(false);
    return res;
}

SubGhzProtocolStatus
    ws_block_generic_deserialize(WSBlockGeneric* instance, FlipperFormat* flipper_format) {
    furi_assert(instance);
    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    uint32_t temp_data = 0;

    do {
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        if(!flipper_format_read_uint32(flipper_format, "Id", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Id");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->id = (uint32_t)temp_data;

        if(!flipper_format_read_uint32(flipper_format, "Bit", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Bit");
            res = SubGhzProtocolStatusErrorParserBitCount;
            break;
        }
        instance->data_count_bit = (uint8_t)temp_data;

        uint8_t key_data[sizeof(uint64_t)] = {0};
        if(!flipper_format_read_hex(flipper_format, "Data", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Missing Data");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        for(uint8_t i = 0; i < sizeof(uint64_t); i++) {
            instance->data = instance->data << 8 | key_data[i];
        }

        if(!flipper_format_read_uint32(flipper_format, "Batt", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Battery_low");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->battery_low = (uint8_t)temp_data;

        if(!flipper_format_read_uint32(flipper_format, "Hum", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Humidity");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->humidity = (uint8_t)temp_data;

        if(!flipper_format_read_uint32(flipper_format, "Ts", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing timestamp");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->timestamp = (uint32_t)temp_data;

        if(!flipper_format_read_uint32(flipper_format, "Ch", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Channel");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->channel = (uint8_t)temp_data;

        if(!flipper_format_read_uint32(flipper_format, "Btn", (uint32_t*)&temp_data, 1)) {
            FURI_LOG_E(TAG, "Missing Btn");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->btn = (uint8_t)temp_data;

        float temp;
        if(!flipper_format_read_float(flipper_format, "Temp", (float*)&temp, 1)) {
            FURI_LOG_E(TAG, "Missing Temperature");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->temp = temp;

        res = SubGhzProtocolStatusOk;
    } while(0);

    return res;
}

SubGhzProtocolStatus ws_block_generic_deserialize_check_count_bit(
    WSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    uint16_t count_bit) {
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = ws_block_generic_deserialize(instance, flipper_format);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(instance->data_count_bit != count_bit) {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            ret = SubGhzProtocolStatusErrorValueBitCount;
            break;
        }
    } while(false);
    return ret;
}

void ws_block_generic_get_string(WSBlockGeneric* instance, FuriString* output) {
    furi_string_cat_printf(
        output, "%s\r\n%dbit", instance->protocol_name, instance->data_count_bit);
    if(instance->channel != WS_NO_CHANNEL) {
        furi_string_cat_printf(output, "   Ch: %01d", instance->channel);
    }
    if(instance->btn != WS_NO_BTN) {
        furi_string_cat_printf(output, "   Btn: %01d\r\n", instance->btn);
    } else {
        furi_string_cat(output, "\r\n");
    }

    if(instance->id != WS_NO_ID) {
        furi_string_cat_printf(output, "Sn: 0x%02lX   ", instance->id);
    }
    if(instance->battery_low != WS_NO_BATT) {
        furi_string_cat_printf(output, "Batt: %s\r\n", (!instance->battery_low ? "ok" : "low"));
    } else {
        furi_string_cat(output, "\r\n");
    }

    furi_string_cat_printf(
        output,
        "Data: 0x%lX%08lX\r\n",
        (uint32_t)(instance->data >> 32),
        (uint32_t)(instance->data));

    if(!float_is_equal(instance->temp, WS_NO_TEMPERATURE)) {
        bool is_metric = furi_hal_rtc_get_locale_units() == FuriHalRtcLocaleUnitsMetric;
        furi_string_cat_printf(
            output,
            "Temp: %3.1f%c   ",
            (double)(is_metric ? instance->temp : locale_celsius_to_fahrenheit(instance->temp)),
            is_metric ? 'C' : 'F');
    }
    if(instance->humidity != WS_NO_HUMIDITY) {
        furi_string_cat_printf(output, "Hum: %d%%", instance->humidity);
    }
}
