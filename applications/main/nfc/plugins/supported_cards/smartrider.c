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

static inline uint8_t get_first_block_num_of_sector(uint8_t sector) {
    return sector * 4;
}

__attribute__((always_inline)) static inline bool authenticate_and_read(
    Nfc* nfc,
    uint8_t sector,
    const uint8_t* key,
    MfClassicKeyType key_type,
    MfClassicBlock* block_data) {
    MfClassicKey mf_key = {{0}};
    __builtin_memcpy(mf_key.data, key, 6);

    uint8_t block = get_first_block_num_of_sector(sector);

    if(mf_classic_poller_sync_auth(nfc, block, &mf_key, key_type, NULL) != MfClassicErrorNone) {
        return false;
    }

    return mf_classic_poller_sync_read_block(nfc, block, &mf_key, key_type, block_data) ==
           MfClassicErrorNone;
}

__attribute__((hot)) static bool smartrider_verify(Nfc* nfc) {
    furi_assert(nfc);
    MfClassicBlock block_data;

    static const uint8_t sectors[] = {0, 6, 12};
    static const MfClassicKeyType key_types[] = {
        MfClassicKeyTypeA, MfClassicKeyTypeB, MfClassicKeyTypeA};

    for(uint_fast8_t i = 0; i < 3; i++) {
        if(!authenticate_and_read(nfc, sectors[i], STANDARD_KEYS[i], key_types[i], &block_data)) {
            FURI_LOG_D(TAG, "Authentication or read failed for key %d", i);
            return false;
        }
        if(__builtin_memcmp(block_data.data, STANDARD_KEYS[i], 6) != 0) {
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
static inline void set_key(MfClassicKey* dst, const uint8_t* src) {
    memcpy(dst->data, src, sizeof(dst->data));
}

__attribute__((hot)) static bool smartrider_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    MfClassicType type;
    if(mf_classic_poller_sync_detect_type(nfc, &type) != MfClassicErrorNone ||
       type != MfClassicType1k) {
        mf_classic_free(data);
        return false;
    }
    data->type = type;

    MfClassicDeviceKeys keys = {0};
    const size_t total_sectors = mf_classic_get_total_sectors_num(type);

    // Set keys for all sectors
    for(size_t i = 0; i < total_sectors; i++) {
        set_key(&keys.key_a[i], STANDARD_KEYS[i == 0 ? 0 : 1]);
        if(i > 0) {
            set_key(&keys.key_b[i], STANDARD_KEYS[2]);
            FURI_BIT_SET(keys.key_b_mask, i);
        }
        FURI_BIT_SET(keys.key_a_mask, i);
    }

    MfClassicError error = mf_classic_poller_sync_read(nfc, &keys, data);

    if(error != MfClassicErrorNone) {
        if(error == MfClassicErrorNotPresent) {
            FURI_LOG_W(TAG, "Failed to read data");
        }
        mf_classic_free(data);
        return false;
    }

    nfc_device_set_data(device, NfcProtocolMfClassic, data);
    mf_classic_free(data);
    return true;
}

static inline uint16_t read_le16(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline void format_hex(char* dst, const uint8_t* src, size_t len) {
    static const char hex_chars[] = "0123456789ABCDEF";
    for(size_t i = 0; i < len; i++) {
        dst[i * 2] = hex_chars[src[i] >> 4];
        dst[i * 2 + 1] = hex_chars[src[i] & 0xF];
    }
}

static inline uint8_t days_in_month(uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month - 1];
}

__attribute__((hot)) static bool
    smartrider_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    // Verify key
    const MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(data, 0);
    if(!sec_tr || memcmp(sec_tr->key_a.data, STANDARD_KEYS[0], 6) != 0) {
        FURI_LOG_E(TAG, "Key verification failed for sector 0");
        return false;
    }

    // Check if required blocks are read
    static const uint8_t required_blocks[] = {14, 4, 5, 1, 52, 50, 0};
    for(uint_fast8_t i = 0; i < sizeof(required_blocks); i++) {
        if(required_blocks[i] >= MAX_BLOCKS ||
           !mf_classic_is_block_read(data, required_blocks[i])) {
            FURI_LOG_E(TAG, "Required block %d is not read or out of range", required_blocks[i]);
            return false;
        }
    }

    SmartRiderData sr_data = {0};

    // Parse balance and configuration data
    const uint8_t* balance_data = data->block[14].data;
    const uint8_t* config_data = data->block[4].data;
    sr_data.balance = read_le16(balance_data + 7);
    sr_data.issued_days = read_le16(config_data + 16);
    sr_data.expiry_days = read_le16(config_data + 18);
    sr_data.auto_load_threshold = read_le16(config_data + 20);
    sr_data.auto_load_value = read_le16(config_data + 22);

    // Parse current token and purchase cost
    sr_data.token = data->block[5].data[8];
    sr_data.purchase_cost = read_le16(data->block[0].data + 14);

    // Parse card serial number
    format_hex(sr_data.card_serial_number, data->block[1].data + 6, 5);
    sr_data.card_serial_number[10] = '\0';

    // Parse trips
    for(uint_fast8_t block_number = 40; block_number <= 52 && sr_data.trip_count < MAX_TRIPS;
        block_number++) {
        if((block_number != 43 && block_number != 47 && block_number != 51) &&
           mf_classic_is_block_read(data, block_number) &&
           parse_trip_data(
               &data->block[block_number], &sr_data.trips[sr_data.trip_count], block_number)) {
            sr_data.trip_count++;
        }
    }

    // Sort trips by timestamp (most recent first) using insertion sort
    for(size_t i = 1; i < sr_data.trip_count; i++) {
        TripData key = sr_data.trips[i];
        int_fast8_t j = i - 1;
        while(j >= 0 && sr_data.trips[j].timestamp < key.timestamp) {
            sr_data.trips[j + 1] = sr_data.trips[j];
            j--;
        }
        sr_data.trips[j + 1] = key;
    }

    // Format the parsed data
    furi_string_printf(
        parsed_data,
        "\e#SmartRider\nBalance: $%u.%02u\nConcession: %s\nSerial: %s%s\n"
        "Total Cost: $%u.%02u\nAuto-Load: $%u.%02u/$%u.%02u\n\e#Trip History\n",
        sr_data.balance / 100,
        sr_data.balance % 100,
        get_concession_type(sr_data.token),
        memcmp(sr_data.card_serial_number, "00", 2) == 0 ? "SR0" : "",
        memcmp(sr_data.card_serial_number, "00", 2) == 0 ? sr_data.card_serial_number + 2 :
                                                           sr_data.card_serial_number,
        sr_data.purchase_cost / 100,
        sr_data.purchase_cost % 100,
        sr_data.auto_load_threshold / 100,
        sr_data.auto_load_threshold % 100,
        sr_data.auto_load_value / 100,
        sr_data.auto_load_value % 100);

    // Add trip history
    char date_str[9];
    for(size_t i = 0; i < sr_data.trip_count; i++) {
        uint32_t seconds_since_2000 = sr_data.trips[i].timestamp;
        uint32_t days_since_2000 = seconds_since_2000 / 86400;
        uint16_t year = 2000 + days_since_2000 / 365;
        uint8_t month = 1;
        uint16_t day = days_since_2000 % 365 + 1;

        // Simple date calculation (not accounting for leap years)
        while(day > 28) {
            uint8_t days_in_month = days_in_month(month);
            if(day <= days_in_month) break;
            day -= days_in_month;
            if(++month > 12) {
                month = 1;
                year++;
            }
        }

        // Manually format the date string as dd/mm/yy
        date_str[0] = '0' + (day / 10);
        date_str[1] = '0' + (day % 10);
        date_str[2] = '/';
        date_str[3] = '0' + month / 10;
        date_str[4] = '0' + month % 10;
        date_str[5] = '/';
        date_str[6] = '0' + (year % 100) / 10;
        date_str[7] = '0' + (year % 100) % 10;
        date_str[8] = '\0';

        uint32_t cost = sr_data.trips[i].cost;
        if(cost > 0) {
            furi_string_cat_printf(
                parsed_data,
                "%s %c $%u.%02u %s\n",
                date_str,
                sr_data.trips[i].tap_on ? '+' : '-',
                cost / 100,
                cost % 100,
                sr_data.trips[i].route);
        } else {
            furi_string_cat_printf(
                parsed_data,
                "%s %c %s\n",
                date_str,
                sr_data.trips[i].tap_on ? '+' : '-',
                sr_data.trips[i].route);
        }
    }

    return true;
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
