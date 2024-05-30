/*
* Parser for CSC Service Works Reloadable Cash Card (US)
* Date created 2024/5/26
* Zinong Li  
* Discord  @torron0483 
* Github   @zinongli
*/

#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <bit_lib.h>
#include <furi/core/string.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

#define TAG "CSC"
#define csc_MAGIC_NUMBER 0x21

typedef struct {
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

static const MfClassicKeyPair csc_1k_keys[] = {
    {.a = 0xF329F7AAEDFF, .b = 0xffffffffffff},
    {.a = 0xF329F7AAEDFF, .b = 0xffffffffffff},
    {.a = 0xF329F7AAEDFF, .b = 0xffffffffffff},
    {.a = 0xF329F7AAEDFF, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xacffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
};

static bool csc_verify(Nfc* nfc) {
    bool verified = false;

    do {
        const uint8_t verify_sector = 1;
        const uint8_t verify_block = mf_classic_get_first_block_num_of_sector(verify_sector) + 1;
        FURI_LOG_D(TAG, "Verifying sector %u", verify_sector);

        MfClassicKey key = {0};
        bit_lib_num_to_bytes_be(csc_1k_keys[verify_sector].a, COUNT_OF(key.data), key.data);

        MfClassicAuthContext auth_context;
        MfClassicError error =
            mf_classic_poller_sync_auth(nfc, verify_block, &key, MfClassicKeyTypeA, &auth_context);
        if(error != MfClassicErrorNone) {
            FURI_LOG_D(TAG, "Failed to read block %u: %d", verify_block, error);
            break;
        }
        verified = true;
    } while(false);

    return verified;
}

static bool csc_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);

    bool is_read = false;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    do {
        MfClassicType type = MfClassicType1k;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error != MfClassicErrorNone) break;

        data->type = type;
        if(type != MfClassicType1k) break;

        MfClassicDeviceKeys keys = {
            .key_a_mask = 0,
            .key_b_mask = 0,
        };
        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            bit_lib_num_to_bytes_be(csc_1k_keys[i].a, sizeof(MfClassicKey), keys.key_a[i].data);
            FURI_BIT_SET(keys.key_a_mask, i);
            bit_lib_num_to_bytes_be(csc_1k_keys[i].b, sizeof(MfClassicKey), keys.key_b[i].data);
            FURI_BIT_SET(keys.key_b_mask, i);
        }

        error = mf_classic_poller_sync_read(nfc, &keys, data);
        if(error == MfClassicErrorNotPresent) {
            FURI_LOG_W(TAG, "Failed to read data");
            break;
        }

        nfc_device_set_data(device, NfcProtocolMfClassic, data);

        is_read = (error == MfClassicErrorNone);
    } while(false);

    mf_classic_free(data);

    return is_read;
}

bool csc_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
    bool parsed = false;

    do {
        // Verify key
        const uint8_t ticket_sector_number = 1;

        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, ticket_sector_number);

        const uint64_t key =
            bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key != csc_1k_keys[ticket_sector_number].a) break;
        // Parse data
        const uint8_t refill_block_num = 2;
        const uint8_t current_balance_block_num = 4;
        const uint8_t card_lives_block_num = 9;
        const uint8_t refill_sign_block_num = 13;
        const uint64_t new_card_sign = 0x00000000000000;

        const uint8_t* refilled_balance_block_start_ptr = &data->block[refill_block_num].data[9];
        const uint8_t* refill_times_block_start_ptr = &data->block[refill_block_num].data[5];
        const uint8_t* current_balance_block_start_ptr =
            &data->block[current_balance_block_num].data[0];
        const uint8_t* card_lives_block_start_ptr = &data->block[card_lives_block_num].data[0];
        const uint8_t* refill_sign_block_start_ptr = &data->block[refill_sign_block_num].data[0];

        uint32_t refilled_balance = bit_lib_bytes_to_num_le(refilled_balance_block_start_ptr, 2);
        uint32_t refilled_balance_dollar = refilled_balance / 100;
        uint8_t refilled_balance_cent = refilled_balance % 100;

        uint32_t current_balance = bit_lib_bytes_to_num_le(current_balance_block_start_ptr, 2);
        uint32_t current_balance_dollar = current_balance / 100;
        uint8_t current_balance_cent = current_balance % 100;

        uint32_t card_lives = bit_lib_bytes_to_num_le(card_lives_block_start_ptr, 2);
        uint32_t refill_times = bit_lib_bytes_to_num_le(refill_times_block_start_ptr, 2);

        uint64_t refill_sign = bit_lib_bytes_to_num_le(refill_sign_block_start_ptr, 8);

        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        uint32_t card_uid = bit_lib_bytes_to_num_le(uid, 4);

        if(refill_sign == new_card_sign) {
            furi_string_printf(
                parsed_data,
                "\e#CSC Service Works\nUID: %lu\nNew Card\nCard Value: %lu.%02u USD\nCard Usages Left: %lu",
                card_uid,
                refilled_balance_dollar,
                refilled_balance_cent,
                card_lives);
        } else {
            furi_string_printf(
                parsed_data,
                "\e#CSC Service Works\nUID: %lu\nBalance: %lu.%02u USD\nLast Top-up: %lu.%02u USD\nTop-up Count: %lu\nCard Usages Left: %lu",
                card_uid,
                current_balance_dollar,
                current_balance_cent,
                refilled_balance_dollar,
                refilled_balance_cent,
                refill_times,
                card_lives);
        }
        parsed = true;
    } while(false);

    return parsed;
}

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin csc_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = csc_verify,
    .read = csc_read,
    .parse = csc_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor csc_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &csc_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* csc_plugin_ep(void) {
    return &csc_plugin_descriptor;
}
