#include "nfc_supported_card_plugin.h"
#include <bit_lib.h>
#include <flipper_application.h>
#include <furi.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <string.h>

#define TAG "SmartRider"

static const uint8_t STANDARD_KEYS[3][6] = {
    {0x20, 0x31, 0xD1, 0xE5, 0x7A, 0x3B},
    {0x4C, 0xA6, 0x02, 0x9F, 0x94, 0x73},
    {0x19, 0x19, 0x53, 0x98, 0xE3, 0x2F}};

typedef struct {
    uint32_t timestamp;
    uint8_t tap_on;
    char route[5];
    uint32_t cost;
    uint16_t transaction_number;
    uint16_t journey_number;
} TripData;

typedef struct {
    uint32_t balance;
    uint8_t token;
    uint16_t issued_days;
    uint16_t expiry_days;
    char card_serial_number[11];
    uint16_t purchase_cost;
    uint16_t auto_load_threshold;
    uint16_t auto_load_value;
    TripData last_trips[2];
} SmartRiderData;

static int offset_to_block(int offset) {
    return offset / 16;
}

static int offset_in_block(int offset) {
    return offset % 16;
}

static const char* get_concession_type(uint8_t token) {
    switch(token) {
    case 0x00:
        return "Pre-issue";
    case 0x01:
        return "Standard Fare";
    case 0x02:
        return "Student";
    case 0x04:
        return "Tertiary";
    case 0x06:
        return "Seniors";
    case 0x07:
        return "Health Care";
    case 0x0e:
        return "PTA Staff";
    case 0x0f:
        return "Pensioner";
    case 0x10:
        return "Free Travel";
    default:
        return "Unknown";
    }
}

static bool authenticate_and_read(
    Nfc* nfc,
    uint8_t sector,
    const uint8_t* key,
    MfClassicKeyType key_type,
    MfClassicBlock* block_data) {
    MfClassicKey mf_key = {};
    memcpy(mf_key.data, key, 6);
    MfClassicError error = mf_classic_poller_sync_auth(
        nfc, mf_classic_get_first_block_num_of_sector(sector), &mf_key, key_type, NULL);
    if(error != MfClassicErrorNone) {
        FURI_LOG_D(TAG, "Authentication failed for sector %d key type %d", sector, key_type);
        return false;
    }
    error = mf_classic_poller_sync_read_block(
        nfc, mf_classic_get_first_block_num_of_sector(sector), &mf_key, key_type, block_data);
    if(error != MfClassicErrorNone) {
        FURI_LOG_D(TAG, "Read failed for sector %d", sector);
        return false;
    }
    return true;
}

static bool smartrider_verify(Nfc* nfc) {
    furi_assert(nfc);
    MfClassicBlock block_data;

    // Authenticate and read blocks for each standard key
    for(int i = 0; i < 3; i++) {
        if(!authenticate_and_read(
               nfc,
               i * 6,
               STANDARD_KEYS[i],
               i % 2 == 0 ? MfClassicKeyTypeA : MfClassicKeyTypeB,
               &block_data)) {
            FURI_LOG_D(TAG, "Authentication or read failed for key %d", i);
            return false;
        }
        if(memcmp(block_data.data, STANDARD_KEYS[i], 6) != 0) {
            FURI_LOG_D(TAG, "Key mismatch for key %d", i);
            return false;
        }
    }

    FURI_LOG_I(TAG, "SmartRider card verified");
    return true;
}

static bool parse_trip_data(const MfClassicBlock* block_data, TripData* trip, int trip_offset) {
    trip->timestamp = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 3, 4);
    trip->tap_on = (block_data->data[trip_offset + 7] & 0x10) == 0x10;
    memcpy(trip->route, block_data->data + trip_offset + 8, 4);
    trip->route[4] = '\0';
    trip->cost = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 13, 2);
    trip->transaction_number = bit_lib_bytes_to_num_le(block_data->data + trip_offset, 2);
    trip->journey_number = bit_lib_bytes_to_num_le(block_data->data + trip_offset + 2, 2);
    return true;
}

static bool smartrider_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);
    bool is_read = false;
    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    MfClassicType type = MfClassicTypeMini;
    MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
    if(error != MfClassicErrorNone) {
        mf_classic_free(data);
        return false;
    }

    data->type = type;
    if(type != MfClassicType1k) {
        mf_classic_free(data);
        return false;
    }

    MfClassicDeviceKeys keys = {.key_a_mask = 0, .key_b_mask = 0};

    // Pre-set all keys to the default for sectors beyond the first
    for(size_t i = 1; i < mf_classic_get_total_sectors_num(data->type); i++) {
        memcpy(keys.key_a[i].data, STANDARD_KEYS[1], sizeof(STANDARD_KEYS[1]));
        memcpy(keys.key_b[i].data, STANDARD_KEYS[2], sizeof(STANDARD_KEYS[2]));
        FURI_BIT_SET(keys.key_a_mask, i);
        FURI_BIT_SET(keys.key_b_mask, i);
    }

    // Set key for the first sector
    memcpy(keys.key_a[0].data, STANDARD_KEYS[0], sizeof(STANDARD_KEYS[0]));
    FURI_BIT_SET(keys.key_a_mask, 0);

    error = mf_classic_poller_sync_read(nfc, &keys, data);
    if(error == MfClassicErrorNotPresent) {
        FURI_LOG_W(TAG, "Failed to read data");
        mf_classic_free(data);
        return false;
    }

    nfc_device_set_data(device, NfcProtocolMfClassic, data);
    is_read = (error == MfClassicErrorNone);
    mf_classic_free(data);
    return is_read;
}

static bool smartrider_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
    bool parsed = false;
    do {
        // Verify key
        const uint8_t verify_sector = 0; // We'll check sector 0 for SmartRider
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, verify_sector);
        if(sec_tr == NULL) {
            FURI_LOG_E(TAG, "Failed to get sector trailer for sector %d", verify_sector);
            break;
        }
        const uint64_t key =
            bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key != bit_lib_bytes_to_num_be(STANDARD_KEYS[0], sizeof(STANDARD_KEYS[0]))) {
            FURI_LOG_E(TAG, "Key mismatch for sector 0");
            break;
        }

        // Check if required blocks are read
        const uint8_t required_blocks[] = {
            offset_to_block(0xe0),
            offset_to_block(0x40),
            offset_to_block(0x50),
            1,
            offset_to_block(0x340),
            offset_to_block(0x320),
            offset_to_block(0x06)};
        for(size_t i = 0; i < COUNT_OF(required_blocks); i++) {
            if(!mf_classic_is_block_read(data, required_blocks[i])) {
                FURI_LOG_E(TAG, "Required block %d is not read", required_blocks[i]);
                break;
            }
        }

        SmartRiderData sr_data = {0};

        // Parse balance
        const uint8_t balance_block = offset_to_block(0xe0);
        const uint8_t* block_data = data->block[balance_block].data;
        sr_data.balance = bit_lib_bytes_to_num_le(block_data + offset_in_block(0xe0) + 7, 2);

        // Parse configuration data (Sector 1)
        const uint8_t config_block = offset_to_block(0x40);
        block_data = data->block[config_block].data;
        sr_data.issued_days = bit_lib_bytes_to_num_le(block_data + 16, 2);
        sr_data.expiry_days = bit_lib_bytes_to_num_le(block_data + 18, 2);
        sr_data.auto_load_threshold = bit_lib_bytes_to_num_le(block_data + 20, 2);
        sr_data.auto_load_value = bit_lib_bytes_to_num_le(block_data + 22, 2);

        // Parse current token
        const uint8_t token_block = offset_to_block(0x50);
        block_data = data->block[token_block].data;
        sr_data.token = block_data[8];

        // Parse card serial number
        block_data = data->block[1].data;
        snprintf(
            sr_data.card_serial_number,
            sizeof(sr_data.card_serial_number),
            "%02X%02X%02X%02X%02X",
            block_data[6],
            block_data[7],
            block_data[8],
            block_data[9],
            block_data[10]);

        // Parse purchase cost
        const uint8_t purchase_block = offset_to_block(0x06);
        block_data = data->block[purchase_block].data;
        sr_data.purchase_cost = bit_lib_bytes_to_num_le(block_data + offset_in_block(0x06) + 8, 2);

        // Parse last two trips
        const uint16_t trip_offsets[] = {0x340, 0x320};
        for(int i = 0; i < 2; i++) {
            const uint8_t trip_block = offset_to_block(trip_offsets[i]);
            block_data = data->block[trip_block].data;
            if(!parse_trip_data(
                   &data->block[trip_block],
                   &sr_data.last_trips[i],
                   offset_in_block(trip_offsets[i]))) {
                FURI_LOG_E(TAG, "Failed to parse trip data for offset 0x%03X", trip_offsets[i]);
                break;
            }
        }

        furi_string_printf(
            parsed_data,
            "\e#SmartRider\n"
            "Balance: $%lu.%02lu\n"
            "Concession: %s\n"
            "Serial: %s%s\n"
            "Total Cost: $%u.%02u\n"
            "Auto-Load: $%u.%02u/$%u.%02u\n"
            "Last Trip: %s $%lu.%02lu %s\n"
            "Prev Trip: %s $%lu.%02lu %s",
            sr_data.balance / 100,
            sr_data.balance % 100,
            get_concession_type(sr_data.token),
            strncmp(sr_data.card_serial_number, "00", 2) == 0 ? "SR0" : "",
            strncmp(sr_data.card_serial_number, "00", 2) == 0 ? sr_data.card_serial_number + 2 :
                                                                sr_data.card_serial_number,
            sr_data.purchase_cost / 100,
            sr_data.purchase_cost % 100,
            sr_data.auto_load_threshold / 100,
            sr_data.auto_load_threshold % 100,
            sr_data.auto_load_value / 100,
            sr_data.auto_load_value % 100,
            sr_data.last_trips[0].tap_on ? "Tag on" : "Tag off",
            (unsigned long)(sr_data.last_trips[0].cost / 100),
            (unsigned long)(sr_data.last_trips[0].cost % 100),
            sr_data.last_trips[0].route,
            sr_data.last_trips[1].tap_on ? "Tag on" : "Tag off",
            (unsigned long)(sr_data.last_trips[1].cost / 100),
            (unsigned long)(sr_data.last_trips[1].cost % 100),
            sr_data.last_trips[1].route);

        parsed = true;
    } while(false);
    return parsed;
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
