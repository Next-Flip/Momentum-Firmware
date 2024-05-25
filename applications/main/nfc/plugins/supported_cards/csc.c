#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <bit_lib.h>
#include <furi/core/string.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

#define TAG "csc"

#define csc_MAGIC_NUMBER 0x21  // Replace with your card's unique identifier
//#define csc_VERSION 0x01       // Replace with your card's version if applicable


static bool csc_verify(Nfc* nfc) {
    //MfClassicData* data = mf_classic_alloc();
    //furi_assert(nfc);
    //furi_assert(device);
    //nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    bool is_verified = false;

    do {
        MfClassicType type = MfClassicType1k;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error != MfClassicErrorNone || type != MfClassicType1k) {
            FURI_LOG_D(TAG,"Card type detection failed or not Mifare Classic 1k\n");
            break;
        }
        //data->type = type;
        if(type != MfClassicType1k) break;

        //block_data = data->block[0].data[0];
        //printf("Block 0 Data: %02X %02X %02X %02X\n", block_data[0], block_data[1], block_data[2], block_data[3]);
        //if(block_data[0] == MYCARD_MAGIC_NUMBER) {
            is_verified = true;
        //} else {
        //    furi_string_printf("Verification failed: Magic number or version mismatch\n");
        //}
        FURI_LOG_D(TAG,"works as of verification");
    } while(false);

    //mf_classic_free(data);
    return is_verified;
}

bool csc_read(Nfc* nfc, NfcDevice* device) {
    // Custom read procedure for Mifare Classic 1k
    furi_assert(nfc);
    furi_assert(device);
    return true;
}

bool csc_parse(const NfcDevice* device, FuriString* parsed_data) {
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
    bool parsed = false;

    do {
        // Verify key
        
        // Parse data
        const uint8_t last_refill_block_num = 2;
        const uint8_t current_balance_block_num = 4;
        const uint8_t card_lives_block_num = 9;

        const uint8_t* last_refill_block_start_ptr =
            &data->block[last_refill_block_num].data[9];
        const uint8_t* current_balance_block_start_ptr =
            &data->block[current_balance_block_num].data[0];
        const uint8_t* card_lives_block_start_ptr =
            &data->block[card_lives_block_num].data[0];

        uint32_t last_refill_balance = bit_lib_bytes_to_num_le(last_refill_block_start_ptr, 2);
        uint32_t last_refill_balance_lari = last_refill_balance / 100;
        uint8_t last_refill_balance_tetri = last_refill_balance % 100;

        uint32_t current_balance = bit_lib_bytes_to_num_le(current_balance_block_start_ptr, 2);
        uint32_t current_balance_lari = current_balance / 100;
        uint8_t current_balance_tetri = current_balance % 100;

        uint32_t card_lives = bit_lib_bytes_to_num_le(card_lives_block_start_ptr, 2);

        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        uint32_t card_number = bit_lib_bytes_to_num_le(uid, 4);

        furi_string_printf(
            parsed_data,
            "\e#CSC Service Works\nCard number: %lu\nBalance: %lu.%02u USD\nLast Refill Balance: %lu.%02u USD\nCard Usage Left: %lu",
            card_number,
            current_balance_lari,
            current_balance_tetri,
            last_refill_balance_lari,
            last_refill_balance_tetri,
            card_lives);
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
