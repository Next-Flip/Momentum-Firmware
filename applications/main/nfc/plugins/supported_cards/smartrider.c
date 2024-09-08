#include "nfc_supported_card_plugin.h"
#include <bit_lib.h>
#include <flipper_application.h>
#include <furi.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <string.h>
#define MAX_TRIPS           10
#define TAG                 "SmartRider"
#define MAX_BLOCKS          64
#define MAX_DATE_ITERATIONS 366

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
    uint8_t block;
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
    TripData trips[MAX_TRIPS];
    size_t trip_count;
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

static bool
    parse_trip_data(const MfClassicBlock* block_data, TripData* trip, uint8_t block_number) {
    trip->timestamp = bit_lib_bytes_to_num_le(block_data->data + 3, 4);
    trip->tap_on = (block_data->data[7] & 0x10) == 0x10;
    memcpy(trip->route, block_data->data + 8, 4);
    trip->route[4] = '\0';
    trip->cost = bit_lib_bytes_to_num_le(block_data->data + 13, 2);
    trip->transaction_number = bit_lib_bytes_to_num_le(block_data->data, 2);
    trip->journey_number = bit_lib_bytes_to_num_le(block_data->data + 2, 2);
    trip->block = block_number;
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
    bool error_occurred = false;

    do {
        // Verify key
        const uint8_t verify_sector = 0; // We'll check sector 0 for SmartRider
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, verify_sector);
        if(sec_tr == NULL) {
            FURI_LOG_E(TAG, "Failed to get sector trailer for sector %d", verify_sector);
            error_occurred = true;
            break;
        }
        const uint64_t key =
            bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key != bit_lib_bytes_to_num_be(STANDARD_KEYS[0], sizeof(STANDARD_KEYS[0]))) {
            FURI_LOG_E(TAG, "Key mismatch for sector 0");
            error_occurred = true;
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
            if(required_blocks[i] >= MAX_BLOCKS ||
               !mf_classic_is_block_read(data, required_blocks[i])) {
                FURI_LOG_E(
                    TAG, "Required block %d is not read or out of range", required_blocks[i]);
                error_occurred = true;
                break;
            }
        }
        if(error_occurred) break;

        SmartRiderData sr_data = {0};

        // Parse balance
        const uint8_t balance_block = offset_to_block(0xe0);
        if(balance_block < MAX_BLOCKS) {
            const uint8_t* block_data = data->block[balance_block].data;
            sr_data.balance = bit_lib_bytes_to_num_le(block_data + offset_in_block(0xe0) + 7, 2);
        } else {
            FURI_LOG_E(TAG, "Balance block out of range");
            error_occurred = true;
            break;
        }

        // Parse configuration data (Sector 1)
        const uint8_t config_block = offset_to_block(0x40);
        if(config_block < MAX_BLOCKS) {
            const uint8_t* block_data = data->block[config_block].data;
            sr_data.issued_days = bit_lib_bytes_to_num_le(block_data + 16, 2);
            sr_data.expiry_days = bit_lib_bytes_to_num_le(block_data + 18, 2);
            sr_data.auto_load_threshold = bit_lib_bytes_to_num_le(block_data + 20, 2);
            sr_data.auto_load_value = bit_lib_bytes_to_num_le(block_data + 22, 2);
        } else {
            FURI_LOG_E(TAG, "Config block out of range");
            error_occurred = true;
            break;
        }

        // Parse current token
        const uint8_t token_block = offset_to_block(0x50);
        if(token_block < MAX_BLOCKS) {
            const uint8_t* block_data = data->block[token_block].data;
            sr_data.token = block_data[8];
        } else {
            FURI_LOG_E(TAG, "Token block out of range");
            error_occurred = true;
            break;
        }

        // Parse card serial number
        if(1 < MAX_BLOCKS) {
            const uint8_t* block_data = data->block[1].data;
            snprintf(
                sr_data.card_serial_number,
                sizeof(sr_data.card_serial_number),
                "%02X%02X%02X%02X%02X",
                block_data[6],
                block_data[7],
                block_data[8],
                block_data[9],
                block_data[10]);
        } else {
            FURI_LOG_E(TAG, "Serial number block out of range");
            error_occurred = true;
            break;
        }

        // Parse purchase cost
        const uint8_t purchase_block = offset_to_block(0x06);
        if(purchase_block < MAX_BLOCKS) {
            const uint8_t* block_data = data->block[purchase_block].data;
            sr_data.purchase_cost =
                bit_lib_bytes_to_num_le(block_data + offset_in_block(0x06) + 8, 2);
        } else {
            FURI_LOG_E(TAG, "Purchase cost block out of range");
            error_occurred = true;
            break;
        }

        // Parse trips
        sr_data.trip_count = 0;
        for(uint8_t block_number = 40; block_number <= 52 && sr_data.trip_count < MAX_TRIPS;
            block_number++) {
            if(block_number == 43 || block_number == 47 || block_number == 51) {
                continue; // Skip these blocks as they contain incorrect data
            }
            if(block_number >= MAX_BLOCKS || !mf_classic_is_block_read(data, block_number)) {
                continue; // Skip unread or out of range blocks
            }
            const MfClassicBlock* block_data = &data->block[block_number];
            if(parse_trip_data(block_data, &sr_data.trips[sr_data.trip_count], block_number)) {
                sr_data.trip_count++;
            }
        }

        // Sort trips by timestamp (most recent first)
        for(size_t i = 0; i < sr_data.trip_count - 1; i++) {
            for(size_t j = 0; j < sr_data.trip_count - i - 1; j++) {
                if(sr_data.trips[j].timestamp < sr_data.trips[j + 1].timestamp) {
                    TripData temp = sr_data.trips[j];
                    sr_data.trips[j] = sr_data.trips[j + 1];
                    sr_data.trips[j + 1] = temp;
                }
            }
        }

        // Format the parsed data
        furi_string_printf(
            parsed_data,
            "\e#SmartRider\n"
            "Balance: $%lu.%02lu\n"
            "Concession: %s\n"
            "Serial: %s%s\n"
            "Total Cost: $%u.%02u\n"
            "Auto-Load: $%u.%02u/$%u.%02u\n"
            "\e#Trip History\n",
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
            sr_data.auto_load_value % 100);

        // Add trip history
        for(size_t i = 0; i < sr_data.trip_count; i++) {
            char date_str[9]; // dd/mm/yy + null terminator
            uint32_t seconds_since_2000 = sr_data.trips[i].timestamp;
            uint32_t days_since_2000 = seconds_since_2000 / 86400;
            uint32_t year = 2000 + days_since_2000 / 365;
            uint32_t month = 1;
            uint32_t day = days_since_2000 % 365 + 1;

            // Simple date calculation (not accounting for leap years)
            int iterations = 0;
            while(day > 28 && iterations++ < MAX_DATE_ITERATIONS) {
                if(month == 2) {
                    day -= 28;
                    month++;
                } else if((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
                    day -= 30;
                    month++;
                } else if(day > 31) {
                    day -= 31;
                    month++;
                } else {
                    break;
                }
                if(month > 12) {
                    month = 1;
                    year++;
                }
            }

            if(iterations >= MAX_DATE_ITERATIONS) {
                FURI_LOG_E(TAG, "Date calculation error for trip %d", i);
                continue;
            }

            // Manually format the date string as dd/mm/yy
            date_str[0] = '0' + (day / 10);
            date_str[1] = '0' + (day % 10);
            date_str[2] = '/';
            date_str[3] = '0' + (month / 10);
            date_str[4] = '0' + (month % 10);
            date_str[5] = '/';
            date_str[6] = '0' + ((year % 100) / 10);
            date_str[7] = '0' + ((year % 100) % 10);
            date_str[8] = '\0';

            // Determine +/- symbol and whether to display cost
            const char tag_symbol = sr_data.trips[i].tap_on ? '+' : '-';
            uint32_t cost = sr_data.trips[i].cost;

            if(cost > 0) {
                furi_string_cat_printf(
                    parsed_data,
                    "%s %c $%lu.%02lu %s\n",
                    date_str,
                    tag_symbol,
                    (unsigned long)(cost / 100),
                    (unsigned long)(cost % 100),
                    sr_data.trips[i].route);
            } else {
                furi_string_cat_printf(
                    parsed_data, "%s %c %s\n", date_str, tag_symbol, sr_data.trips[i].route);
            }
        }

        parsed = true;
    } while(false);

    return parsed && !error_occurred;
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

// made with love by jay candel <3
