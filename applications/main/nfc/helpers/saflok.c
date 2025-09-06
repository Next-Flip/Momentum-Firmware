// Based on: https://github.com/RfidResearchGroup/proxmark3/blob/master/client/src/cmdhfsaflok.c
// Generation written by Aaron Tulino <me@aaronjamt.com>

#include "saflok.h"

#include <lib/bit_lib/bit_lib.h>

#define ULC_DATA_START_PAGE 34
#define ULC_3DES_START_PAGE 44
#define ULC_DATA_NUM_PAGES  5

static void insert_bits(uint8_t* data, size_t start_bit, size_t num_bits, uint32_t value) {
    for(size_t i = 0; i < num_bits; i++) {
        size_t current_bit = start_bit + i;
        size_t byte_index = current_bit / 8;
        size_t bit_index = 7 - (current_bit % 8);

        uint32_t bit_value = (value >> (num_bits - 1 - i)) & 1U;

        data[byte_index] = (data[byte_index] & ~(1 << bit_index)) | (bit_value << bit_index);
    }
}

// Generates the 17-byte data buffer
void saflok_generate_data(NfcSaflokData* saflok_data, uint8_t* buffer) {
    uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM];
    memset(basicAccess, 0, BASIC_ACCESS_BYTE_NUM);

    insert_bits(basicAccess, 0, 4, saflok_data->card_level);
    insert_bits(basicAccess, 4, 4, saflok_data->card_type);
    insert_bits(basicAccess, 8, 8, saflok_data->card_id);
    insert_bits(basicAccess, 16, 2, saflok_data->opening_key);
    insert_bits(basicAccess, 18, 14, saflok_data->lock_id);
    insert_bits(basicAccess, 32, 12, saflok_data->pass_number);
    insert_bits(basicAccess, 44, 12, saflok_data->sequence_and_combination);
    insert_bits(basicAccess, 56, 1, saflok_data->deadbolt_override);
    insert_bits(basicAccess, 57, 7, saflok_data->restricted_days);
    insert_bits(basicAccess, 116, 12, saflok_data->property_id);

    // Break creation date/time down and shove the bits in the right spots
    uint16_t creation_year = saflok_data->creation.year - SAFLOK_YEAR_OFFSET;
    basicAccess[14] |= creation_year & 0xF0;
    basicAccess[11] = (creation_year << 4) & 0xF0;
    basicAccess[11] |= saflok_data->creation.month & 0x0F;

    basicAccess[12] = (saflok_data->creation.day << 3) & 0xF8;

    basicAccess[12] |= (saflok_data->creation.hour >> 2) & 0x07;
    basicAccess[13] = (saflok_data->creation.hour << 6) & 0xC0;

    basicAccess[13] |= saflok_data->creation.minute & 0x3F;

    // Expiration date is stored as a duration after creation
    // Expiration time is stored as a time-of-day as-is
    uint16_t expire_year = saflok_data->expire.year - saflok_data->creation.year;
    int8_t expire_month = saflok_data->expire.month - saflok_data->creation.month;
    int8_t expire_day = saflok_data->expire.day - saflok_data->creation.day;

    if(expire_month < 0) {
        expire_month += 12;
        expire_year -= 1;
    }

    // Handle day rollover
    // The 0th month is December, to make wrapping around easier
    static const uint8_t days_in_month[] = {31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while(true) {
        uint16_t year = expire_year + saflok_data->creation.year;
        uint8_t month = expire_month + saflok_data->creation.month;
        if(month > 12) month -= 12;

        // minus 1 to get number of days in prior month
        uint8_t max_days = days_in_month[month - 1];
        // Adjust for leap years
        if(month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            max_days = 29;
        }
        if(expire_day >= 0) {
            break;
        }

        expire_day += max_days;
        expire_month--;
        if(expire_month < 0) {
            expire_month += 12;
            expire_year--;
        }
    }
    basicAccess[8] = (expire_year << 4) & 0xF0;
    basicAccess[8] |= expire_month & 0x0F;

    basicAccess[9] = (expire_day << 3) & 0xF8;

    basicAccess[9] |= (saflok_data->expire.hour >> 2) & 0x07;
    basicAccess[10] = (saflok_data->expire.hour & 0x03) << 6;

    basicAccess[10] |= saflok_data->expire.minute & 0x3F;

    // Add checksum and encrypt
    basicAccess[16] = saflok_calculate_checksum(basicAccess);
    saflok_encrypt_card(basicAccess, BASIC_ACCESS_BYTE_NUM, buffer);
}

void saflok_generate_mf_classic(NfcDevice* nfc_device, NfcSaflokData* saflok_data) {
    MfClassicData* mfc_data = mf_classic_alloc();

    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    uid[0] = 0xEB;
    uid[1] = 0xC7;
    uid[2] = 0x04;
    uid[3] = 0x4B;
    mf_classic_set_uid(mfc_data, uid, 4);

    // Generate diversified key from UID
    uint8_t key[6];
    saflok_generate_key(uid, key);
    uint64_t diversified_key = bit_lib_bytes_to_num_be(key, 6);

    // Set up manufacturer block
    mfc_data->iso14443_3a_data->uid_len = 4;
    mfc_data->iso14443_3a_data->atqa[0] = 0x04;
    mfc_data->iso14443_3a_data->atqa[1] = 0x00;
    mfc_data->iso14443_3a_data->sak = 0x08;
    mfc_data->type = MfClassicType1k;
    mf_classic_set_block_read(mfc_data, 0, &mfc_data->block[0]);

    // Fill the remaining blocks
    uint16_t block_num = mf_classic_get_total_block_num(MfClassicType1k);
    for(uint16_t block = 1; block < block_num; block++) {
        if(mf_classic_is_sector_trailer(block)) {
            MfClassicSectorTrailer* sec_tr = (MfClassicSectorTrailer*)mfc_data->block[block].data;
            sec_tr->access_bits.data[0] = 0xFF;
            sec_tr->access_bits.data[1] = 0x07;
            sec_tr->access_bits.data[2] = 0x80;
            sec_tr->access_bits.data[3] = 0x69; // Nice

            uint64_t sector_key = diversified_key;
            if(mf_classic_get_sector_by_block(block) == 1) {
                // Only for sector 1: use the Saflok standard key instead of the diversified key
                sector_key = 0x2a2c13cc242a;
            }

            mf_classic_set_block_read(mfc_data, block, &mfc_data->block[block]);
            mf_classic_set_key_found(
                mfc_data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeA, sector_key);
            mf_classic_set_key_found(
                mfc_data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeB, 0xFFFFFFFFFFFF);

        } else {
            memset(&mfc_data->block[block].data, 0x00, MF_CLASSIC_BLOCK_SIZE);
        }

        mf_classic_set_block_read(mfc_data, block, &mfc_data->block[block]);

        // This is the default log header for cards with no log data
        // 00 00 00 00 00 00 00 00   00 00 00 C1 00 00 00 00
        if(block == 4) {
            mfc_data->block[block].data[11] = 0xC1;
        }
    }

    uint8_t data[BASIC_ACCESS_BYTE_NUM];
    saflok_generate_data(saflok_data, data);

    // Saflok data is stored in block 1 and the first byte of block 2
    memcpy(mfc_data->block[1].data, data, 16);
    mfc_data->block[2].data[0] = data[16];

    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, mfc_data);
    mf_classic_free(mfc_data);
}
