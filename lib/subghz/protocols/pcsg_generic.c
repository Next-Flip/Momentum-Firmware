#include "pcsg_generic.h"
#include <lib/toolbox/stream/stream.h>
#include <lib/flipper_format/flipper_format_i.h>

#define TAG "PCSGBlockGeneric"

SubGhzProtocolStatus pcsg_block_generic_serialize(
    PCSGBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(instance);
    SubGhzProtocolStatus res =
        subghz_block_generic_serialize_common(instance->protocol_name, flipper_format, preset);
    if(res != SubGhzProtocolStatusOk) return res;
    res = SubGhzProtocolStatusError;
    do {
        if(!flipper_format_write_string(flipper_format, "Ric", instance->result_ric)) {
            FURI_LOG_E(TAG, "Unable to add Ric");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        if(!flipper_format_write_string(flipper_format, "Message", instance->result_msg)) {
            FURI_LOG_E(TAG, "Unable to add Message");
            res = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        res = SubGhzProtocolStatusOk;
    } while(false);
    return res;
}

SubGhzProtocolStatus
    pcsg_block_generic_deserialize(PCSGBlockGeneric* instance, FlipperFormat* flipper_format) {
    furi_assert(instance);
    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    FuriString* temp_data = furi_string_alloc();
    FuriString* temp_data2 = furi_string_alloc();

    do {
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        if(!flipper_format_read_string(flipper_format, "Ric", temp_data2)) {
            FURI_LOG_E(TAG, "Missing Ric");
            break;
        }
        if(instance->result_ric != NULL) {
            furi_string_set(instance->result_ric, temp_data2);
        } else {
            instance->result_ric = furi_string_alloc_set(temp_data2);
        }

        if(!flipper_format_read_string(flipper_format, "Message", temp_data)) {
            FURI_LOG_E(TAG, "Missing Message");
            break;
        }
        if(instance->result_msg != NULL) {
            furi_string_set(instance->result_msg, temp_data);
        } else {
            instance->result_msg = furi_string_alloc_set(temp_data);
        }

        res = SubGhzProtocolStatusOk;
    } while(0);

    furi_string_free(temp_data);
    furi_string_free(temp_data2);

    return res;
}
