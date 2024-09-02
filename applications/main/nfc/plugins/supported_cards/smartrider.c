#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <bit_lib.h>
#include <string.h>
#include <furi.h>

#define TAG "SmartRider"

static const uint8_t STANDARD_KEY_1[6] = {0x20, 0x31, 0xD1, 0xE5, 0x7A, 0x3B};

typedef struct {
    uint32_t timestamp;
    uint8_t tap_on;
    char route[5];
    uint32_t cost; // Changed to uint32_t
    uint16_t transaction_number;
    uint16_t journey_number;
} TripData;

typedef struct {
    uint32_t balance;
    uint8_t token;
    uint16_t issued_days;
    uint16_t expiry_days;
    char card_serial_number[11];
    uint32_t purchase_cost;
    TripData last_trips[2];
} SmartRiderData;

static int offset_to_block(int offset) {
    return offset / 16;
}

static int offset_in_block(int offset) {
    return offset % 16;
}

// Forward declaration
static const char* get_concession_type(uint8_t token);

static bool smartrider_verify(Nfc* nfc) {
    uint8_t verify_sector = 0;
    uint8_t block_num = mf_classic_get_first_block_num_of_sector(verify_sector);
    MfClassicKey key = {};
    memcpy(key.data, STANDARD_KEY_1, sizeof(STANDARD_KEY_1));

    MfClassicError error = mf_classic_poller_sync_auth(nfc, block_num, &key, MfClassicKeyTypeA, NULL);
    if (error != MfClassicErrorNone) {
        FURI_LOG_D(TAG, "Authentication failed for block %u: %d", block_num, error);
        return false;
    }

    MfClassicBlock block_data;
    error = mf_classic_poller_sync_read_block(nfc, block_num, &key, MfClassicKeyTypeA, &block_data);
    if (error != MfClassicErrorNone) {
        FURI_LOG_D(TAG, "Failed to read block %u: %d", block_num, error);
        return false;
    }

    return (memcmp(block_data.data, STANDARD_KEY_1, sizeof(STANDARD_KEY_1)) == 0);
}

static bool smartrider_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);

    bool is_read = false;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    do {
        MfClassicType type = MfClassicTypeMini;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error != MfClassicErrorNone) break;

        data->type = type;
        if(type != MfClassicType1k) break;

        MfClassicDeviceKeys keys = {
            .key_a_mask = 0,
            .key_b_mask = 0,
        };
        
        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            memcpy(keys.key_a[i].data, STANDARD_KEY_1, sizeof(STANDARD_KEY_1));
            FURI_BIT_SET(keys.key_a_mask, i);
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

static bool smartrider_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    SmartRiderData sr_data = {0};

    // Parse balance
    const int balance_block = offset_to_block(0xe0);
    const int balance_offset = offset_in_block(0xe0);
    if (mf_classic_is_block_read(data, balance_block)) {
        const MfClassicBlock* block_data = &data->block[balance_block];
        sr_data.balance = bit_lib_bytes_to_num_le(block_data->data + balance_offset + 7, 2);
    } else {
        FURI_LOG_D(TAG, "Balance block not read");
        return false;
    }

    // Parse token and dates
    const int token_block = offset_to_block(0x50);
    const int token_offset = offset_in_block(0x50);
    if (mf_classic_is_block_read(data, token_block)) {
        const MfClassicBlock* block_data = &data->block[token_block];
        sr_data.token = block_data->data[token_offset + 8];
        sr_data.issued_days = bit_lib_bytes_to_num_le(block_data->data + token_offset + 1, 2);
        sr_data.expiry_days = bit_lib_bytes_to_num_le(block_data->data + token_offset + 3, 2);
    } else {
        FURI_LOG_D(TAG, "Token block not read");
        return false;
    }

    // Parse card serial number
    if (mf_classic_is_block_read(data, 1)) {
        const MfClassicBlock* block_data = &data->block[1];
        snprintf(sr_data.card_serial_number, sizeof(sr_data.card_serial_number), "%02X%02X%02X%02X%02X",
                 block_data->data[6], block_data->data[7],
                 block_data->data[8], block_data->data[9],
                 block_data->data[10]);
    } else {
        FURI_LOG_D(TAG, "Serial number block not read");
        return false;
    }

    // Parse purchase cost
    const int purchase_block = offset_to_block(0x06);
    const int purchase_offset = offset_in_block(0x06);
    if (mf_classic_is_block_read(data, purchase_block)) {
        const MfClassicBlock* block_data = &data->block[purchase_block];
        sr_data.purchase_cost = bit_lib_bytes_to_num_le(block_data->data + purchase_offset + 8, 2);
    } else {
        FURI_LOG_D(TAG, "Purchase cost block not read");
        return false;
    }    // Parse last two trips
	
    const int trip_offsets[] = {0x340, 0x320}; // Last two trip offsets
    for (int i = 0; i < 2; i++) {
        const int trip_block = offset_to_block(trip_offsets[i]);
        const int trip_offset = offset_in_block(trip_offsets[i]);
        if (mf_classic_is_block_read(data, trip_block)) {
            const MfClassicBlock* block_data = &data->block[trip_block];
            TripData* trip = &sr_data.last_trips[i];
            trip->timestamp = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 3, 4);
            trip->tap_on = (block_data->data[trip_offset + 7] & 0x10) == 0x10;
            memcpy(trip->route, block_data->data + trip_offset + 8, 4);
            trip->route[4] = '\0';
            trip->cost = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 13, 2);
            trip->transaction_number = bit_lib_bytes_to_num_le(block_data->data + trip_offset, 2);
            trip->journey_number = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 2, 2);
        }
    }

    // Format the parsed data
    furi_string_printf(
        parsed_data,
        "\e#SmartRider Card\n"
        "Balance: $%lu.%02lu\n"
        "Concession: %s\n"
        "Serial: %s\n"
        "Purchase Cost: $%lu.%02lu\n"
        "Last Trip: %s $%lu.%02lu %s (Txn: %u, Jrn: %u)\n"
        "Previous Trip: %s $%lu.%02lu %s (Txn: %u, Jrn: %u)",
        sr_data.balance / 100, sr_data.balance % 100,
        get_concession_type(sr_data.token),
        sr_data.card_serial_number,
        sr_data.purchase_cost / 100, sr_data.purchase_cost % 100,
        sr_data.last_trips[0].tap_on ? "Tag on" : "Tag off",
        (unsigned long)(sr_data.last_trips[0].cost / 100), 
        (unsigned long)(sr_data.last_trips[0].cost % 100),
        sr_data.last_trips[0].route,
        sr_data.last_trips[0].transaction_number,
        sr_data.last_trips[0].journey_number,
        sr_data.last_trips[1].tap_on ? "Tag on" : "Tag off",
        (unsigned long)(sr_data.last_trips[1].cost / 100), 
        (unsigned long)(sr_data.last_trips[1].cost % 100),
        sr_data.last_trips[1].route,
        sr_data.last_trips[1].transaction_number,
        sr_data.last_trips[1].journey_number);

    return true;
}
static const char* get_concession_type(uint8_t token) {
    switch (token) {
        case 0x00: return "Pre-issue";
        case 0x01: return "Standard Fare";
        case 0x02: return "Student";
        case 0x04: return "Tertiary";
        case 0x06: return "Seniors";
        case 0x07: return "Health Care";
        case 0x0e: return "PTA Staff";
        case 0x0f: return "Pensioner";
        case 0x10: return "Free Travel";
        default: return "Unknown";
    }
}

static const NfcSupportedCardsPlugin smartrider_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = smartrider_verify,
    .read = smartrider_read,
    .parse = smartrider_parse,
};

static const FlipperAppPluginDescriptor smartrider_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &smartrider_plugin,
};

const FlipperAppPluginDescriptor* smartrider_plugin_ep(void) {
    return &smartrider_plugin_descriptor;
}