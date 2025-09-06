// FZ plugin by @noproto

// Decryption and parsing from: https://gitee.com/wangshuoyue/unsaflok
// Decryption algorithm and parsing published by Shuoyue Wang
// Parsing also inspired by Lennert Wouters and Ian Carroll's DEFCON 32 talk
// https://defcon.org/html/defcon-32/dc-32-speakers.html
// FZ parser by @Torron, with help from @xtruan, @zacharyweiss, @evilmog and kara (@Arkwin)
#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc_app_i.h>
#include <bit_lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../api/saflok/saflok_util.h"

#define TAG "Saflok"

#define KEY_LENGTH   6
#define UID_LENGTH   4
#define CHECK_SECTOR 1

typedef struct {
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

typedef struct {
    uint8_t level_num;
    char* level_name;
} SaflokKeyLevel;

static SaflokKeyLevel key_levels[] = {
    {1, "Guest Key"},
    {2, "Connectors"},
    {3, "Suite"},
    {4, "Limited Use"},
    {5, "Failsafe"},
    {6, "Inhibit"},
    {7, "Pool/Meeting Master"},
    {8, "Housekeeping"},
    {9, "Floor Key"},
    {10, "Section Key"},
    {11, "Rooms Master"},
    {12, "Grand Master"},
    {13, "Emergency"},
    {14, "Electronic Lockout"},
    {15, "Secondary Programming Key (SPK)"},
    {16, "Primary Programming Key (PPK)"},
};

const char* weekdays[] =
    {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

static MfClassicKeyPair saflok_1k_keys[] = {
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 000
    {.a = 0x2a2c13cc242a, .b = 0xffffffffffff}, // 001
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // 002
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // 003
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 004
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 005
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 006
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 007
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 008
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 009
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 010
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 011
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 012
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 013
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 014
    {.a = 0x000000000000, .b = 0xffffffffffff}, // 015
};

static bool saflok_verify(Nfc* nfc) {
    bool verified = false;

    do {
        const uint8_t block_num = mf_classic_get_first_block_num_of_sector(CHECK_SECTOR);
        FURI_LOG_D(TAG, "Saflok: Verifying sector %i", CHECK_SECTOR);

        MfClassicKey key = {0};
        bit_lib_num_to_bytes_be(saflok_1k_keys[CHECK_SECTOR].a, COUNT_OF(key.data), key.data);

        MfClassicAuthContext auth_context;
        MfClassicError error =
            mf_classic_poller_sync_auth(nfc, block_num, &key, MfClassicKeyTypeA, &auth_context);
        if(error != MfClassicErrorNone) {
            FURI_LOG_D(TAG, "Saflok: Failed to read block %u: %d", block_num, error);
            break;
        }

        verified = true;
    } while(false);

    return verified;
}

static bool saflok_read(Nfc* nfc, NfcDevice* device) {
    FURI_LOG_D(TAG, "Entering Saflok KDF");

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

        size_t uid_len;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        FURI_LOG_D(
            TAG, "Saflok: UID identified: %02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
        if(uid_len != UID_LENGTH) break;

        uint8_t key[KEY_LENGTH];
        saflok_generate_key(uid, key);
        uint64_t num_key = bit_lib_bytes_to_num_be(key, KEY_LENGTH);
        FURI_LOG_D(TAG, "Saflok: Key generated for UID: %012llX", num_key);

        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            if(saflok_1k_keys[i].a == 0x000000000000) {
                saflok_1k_keys[i].a = num_key;
            }
        }

        MfClassicDeviceKeys keys = {};
        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            bit_lib_num_to_bytes_be(saflok_1k_keys[i].a, sizeof(MfClassicKey), keys.key_a[i].data);
            FURI_BIT_SET(keys.key_a_mask, i);
            bit_lib_num_to_bytes_be(saflok_1k_keys[i].b, sizeof(MfClassicKey), keys.key_b[i].data);
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

bool saflok_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
    bool parsed = false;

    do {
        // Check card type
        if(data->type != MfClassicType1k) break;

        // Verify key
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, CHECK_SECTOR);

        const uint64_t key_a =
            bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key_a != saflok_1k_keys[CHECK_SECTOR].a) break;

        // Decrypt basic access
        uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM];
        memcpy(&basicAccess, &data->block[1].data, 16);
        memcpy(&basicAccess[16], &data->block[2].data[0], 1);
        uint8_t decodedBA[BASIC_ACCESS_BYTE_NUM];
        saflok_decrypt_card(basicAccess, BASIC_ACCESS_BYTE_NUM, decodedBA);

        // Byte 0: Key level, LED warning bit, and subgroup functions
        uint8_t key_level = (decodedBA[0] & 0xF0) >> 4;
        uint8_t led_warning = (decodedBA[0] & 0x08) >> 3;

        // Byte 1: Key ID
        uint8_t key_id = decodedBA[1];

        // Byte 2 & 3: KeyRecord, including OpeningKey flag
        uint8_t key_record_high = decodedBA[2] & 0x7F;
        uint8_t opening_key = (decodedBA[2] & 0x80) >> 7;
        uint16_t key_record = (key_record_high << 8) | decodedBA[3];

        // Byte 4 & 5: Pass level in reversed binary
        // This part is commented because the relevance of this info is still unknown
        // uint16_t pass_level = ((decodedBA[4] & 0xFF) << 8) | decodedBA[5];
        // uint8_t pass_levels[12];
        // int pass_levels_count = 0;

        // for (int i = 0; i < 12; i++) {
        //     if ((pass_level >> i) & 1) {
        //         pass_levels[pass_levels_count++] = i + 1;
        //     }
        // }

        // Byte 5 & 6: EncryptSequence + Combination
        uint16_t sequence_combination_number = ((decodedBA[5] & 0x0F) << 8) | decodedBA[6];
        // Bytes 14-15: Property number and year
        uint8_t creation_year_bits = (decodedBA[14] & 0xF0);
        uint16_t property_id = ((decodedBA[14] & 0x0F) << 8) | decodedBA[15];

        // Byte 7: OverrideDeadbolt and Days
        uint8_t override_deadbolt = (decodedBA[7] & 0x80) >> 7;
        uint8_t restricted_weekday = decodedBA[7] & 0x7F;
        // Counter to keep track of the number of restricted days
        int restricted_count = 0;
        // Buffer to store the resulting string
        FuriString* restricted_weekday_string = furi_string_alloc();
        // Check each bit from Monday to Sunday
        for(int i = 0; i < 7; i++) {
            if(restricted_weekday & (0b01000000 >> i)) {
                // If the bit is set, append the corresponding weekday to the buffer
                if(restricted_count > 0) {
                    furi_string_cat_printf(restricted_weekday_string, ", ");
                }
                furi_string_cat_printf(restricted_weekday_string, "%s", weekdays[i]);
                restricted_count++;
            }
        }

        // Determine if all weekdays are restricted
        if(restricted_weekday == 0b01111100) {
            furi_string_printf(restricted_weekday_string, "weekdays");
        }
        // If there are specific restricted days
        else if(restricted_weekday == 0b00000011) {
            furi_string_printf(restricted_weekday_string, "weekends");
        }
        // If no weekdays are restricted
        else if(restricted_weekday == 0) {
            furi_string_printf(restricted_weekday_string, "none");
        }

        // Bytes 8-10: Expiry interval
        uint16_t interval_year = (decodedBA[8] >> 4);
        uint8_t interval_month = decodedBA[8] & 0x0F;
        uint8_t interval_day = (decodedBA[9] >> 3) & 0x1F;
        uint8_t interval_hour = ((decodedBA[9] & 0x07) << 2) | (decodedBA[10] >> 6);
        uint8_t interval_minute = decodedBA[10] & 0x3F;

        // Bytes 11-13: Creation date since 1980 Jan 1st
        uint16_t creation_year =
            (creation_year_bits | ((decodedBA[11] & 0xF0) >> 4)) + SAFLOK_YEAR_OFFSET;
        uint8_t creation_month = decodedBA[11] & 0x0F;
        uint8_t creation_day = (decodedBA[12] >> 3) & 0x1F;
        uint8_t creation_hour = ((decodedBA[12] & 0x07) << 2) | (decodedBA[13] >> 6);
        uint8_t creation_minute = decodedBA[13] & 0x3F;

        uint16_t expire_year = creation_year + interval_year;
        uint8_t expire_month = creation_month + interval_month;
        uint8_t expire_day = creation_day + interval_day;
        uint8_t expire_hour = interval_hour;
        uint8_t expire_minute = interval_minute;

        // Handle month rollover
        while(expire_month > 12) {
            expire_month -= 12;
            expire_year++;
        }

        // Handle day rollover
        static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        while(true) {
            uint8_t max_days = days_in_month[expire_month - 1];
            // Adjust for leap years
            if(expire_month == 2 &&
               (expire_year % 4 == 0 && (expire_year % 100 != 0 || expire_year % 400 == 0))) {
                max_days = 29;
            }
            if(expire_day <= max_days) {
                break;
            }
            expire_day -= max_days;
            expire_month++;
            if(expire_month > 12) {
                expire_month = 1;
                expire_year++;
            }
        }

        // Byte 16: Checksum
        uint8_t checksum = decodedBA[16];
        uint8_t checksum_calculated = saflok_calculate_checksum(decodedBA);
        bool checksum_valid = (checksum_calculated == checksum);
        for(int i = 0; i < 17; i++) {
            FURI_LOG_D(TAG, "%02X", decodedBA[i]);
        }
        FURI_LOG_D(TAG, "CS decrypted: %02X", checksum);
        FURI_LOG_D(TAG, "CS calculated: %02X", checksum_calculated);

        furi_string_cat_printf(parsed_data, "\e#Saflok Card\n");
        furi_string_cat_printf(
            parsed_data,
            "Key Level: %u, %s\n",
            key_levels[key_level].level_num,
            key_levels[key_level].level_name);
        furi_string_cat_printf(parsed_data, "LED Exp. Warning: %s\n", led_warning ? "Yes" : "No");
        furi_string_cat_printf(parsed_data, "Key ID: %02X\n", key_id);
        furi_string_cat_printf(parsed_data, "Key Record: %04X\n", key_record);
        furi_string_cat_printf(parsed_data, "Opening key: %s\n", opening_key ? "Yes" : "No");
        furi_string_cat_printf(
            parsed_data, "Seq. & Combination: %04X\n", sequence_combination_number);
        furi_string_cat_printf(
            parsed_data, "Override Deadbolt: %s\n", override_deadbolt ? "Yes" : "No");
        furi_string_cat_printf(
            parsed_data,
            "Restricted Weekday: %s\n",
            furi_string_get_cstr(restricted_weekday_string));
        furi_string_cat_printf(
            parsed_data,
            "Valid Start Date: \n%u-%02d-%02d\n%02d:%02d:00\n",
            creation_year,
            creation_month,
            creation_day,
            creation_hour,
            creation_minute);
        furi_string_cat_printf(
            parsed_data,
            "Expiration Date: \n%u-%02d-%02d\n%02d:%02d:00\n",
            expire_year,
            expire_month,
            expire_day,
            expire_hour,
            expire_minute);
        furi_string_cat_printf(parsed_data, "Property Number: %u\n", property_id);
        furi_string_cat_printf(parsed_data, "Checksum Valid: %s", checksum_valid ? "Yes" : "No");
        parsed = true;
    } while(false);
    return parsed;
}

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin saflok_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = saflok_verify,
    .read = saflok_read,
    .parse = saflok_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor saflok_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &saflok_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* saflok_plugin_ep(void) {
    return &saflok_plugin_descriptor;
}
