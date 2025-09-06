#include "saflok.h"

#include <lib/bit_lib/bit_lib.h>

#define BASIC_ACCESS_BYTE_NUM 17
#define SAFLOK_YEAR_OFFSET    1980

void generate_saflok_key(const uint8_t* uid, uint8_t* key) {
    static const uint8_t magic_table[192] = {
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xF0, 0x57, 0xB3, 0x9E, 0xE3, 0xD8, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0x96, 0x9D, 0x95, 0x4A, 0xC1, 0x57, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0x8F, 0x43, 0x58, 0x0D, 0x2C, 0x9D, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xFF, 0xCC, 0xE0,
        0x05, 0x0C, 0x43, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x34, 0x1B, 0x15, 0xA6, 0x90, 0xCC,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x89, 0x58, 0x56, 0x12, 0xE7, 0x1B, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0xBB, 0x74, 0xB0, 0x95, 0x36, 0x58, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0xFB, 0x97, 0xF8, 0x4B, 0x5B, 0x74, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xC9, 0xD1, 0x88,
        0x35, 0x9F, 0x92, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x8F, 0x92, 0xE9, 0x7F, 0x58, 0x97,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x16, 0x6C, 0xA2, 0xB0, 0x9F, 0xD1, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0x27, 0xDD, 0x93, 0x10, 0x1C, 0x6C, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0xDA, 0x3E, 0x3F, 0xD6, 0x49, 0xDD, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x58, 0xDD, 0xED,
        0x07, 0x8E, 0x3E, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x5C, 0xD0, 0x05, 0xCF, 0xD9, 0x07,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x11, 0x8D, 0xD0, 0x01, 0x87, 0xD0};

    uint8_t magic_byte = (uid[3] >> 4) + (uid[2] >> 4) + (uid[0] & 0x0F);
    uint8_t magickal_index = (magic_byte & 0x0F) * 12 + 11;

    uint8_t temp_key[6] = {magic_byte, uid[0], uid[1], uid[2], uid[3], magic_byte};
    uint8_t carry_sum = 0;

    for(int i = 6 - 1; i >= 0; i--, magickal_index--) {
        uint16_t keysum = temp_key[i] + magic_table[magickal_index] + carry_sum;
        temp_key[i] = (keysum & 0xFF);
        carry_sum = keysum >> 8;
    }

    memcpy(key, temp_key, 6);
}

unsigned char c_aEncode[256] = {
    236, 116, 192, 99,  86,  153, 105, 100, 159, 23,  38,  198, 240, 1,   16,  77,  202, 82,  138,
    75,  122, 175, 173, 32,  115, 162, 15,  194, 80,  120, 54,  68,  25,  30,  114, 210, 50,  183,
    107, 248, 5,   174, 199, 28,  85,  113, 89,  19,  17,  73,  250, 252, 127, 43,  52,  102, 69,
    165, 185, 21,  169, 163, 134, 150, 219, 45,  218, 208, 33,  84,  189, 227, 131, 141, 110, 155,
    83,  149, 4,   228, 42,  112, 39,  94,  35,  133, 135, 36,  209, 237, 34,  27,  214, 98,  118,
    67,  48,  193, 66,  132, 91,  253, 95,  40,  254, 58,  20,  55,  176, 184, 26,  61,  171, 72,
    251, 152, 3,   166, 119, 201, 11,  117, 97,  8,   241, 245, 217, 121, 101, 172, 229, 164, 223,
    191, 235, 10,  204, 249, 125, 195, 136, 13,  142, 232, 220, 247, 143, 156, 47,  109, 161, 65,
    9,   188, 92,  60,  57,  144, 124, 197, 46,  212, 51,  78,  206, 213, 88,  79,  200, 216, 31,
    130, 22,  62,  215, 255, 190, 146, 157, 196, 211, 14,  29,  181, 93,  24,  7,   126, 106, 243,
    37,  128, 108, 203, 70,  140, 246, 231, 242, 177, 187, 41,  145, 158, 205, 233, 148, 224, 170,
    137, 221, 234, 230, 81,  168, 71,  63,  2,   59,  87,  96,  12,  207, 238, 154, 160, 179, 123,
    225, 147, 186, 178, 182, 222, 0,   226, 167, 139, 76,  53,  74,  111, 239, 18,  129, 44,  180,
    56,  90,  244, 151, 64,  104, 6,   49,  103};

void EncryptCard(unsigned char* keyCard, int length, unsigned char* encryptedCard) {
    int b = 0;
    memcpy(encryptedCard, keyCard, length);
    for(int i = 0; i < length; i++) {
        int b2 = encryptedCard[i];
        int num2 = i;
        for(int j = 0; j < 8; j++) {
            num2 += 1;
            if(num2 >= length) {
                num2 -= length;
            }
            int b3 = encryptedCard[num2];
            int b4 = b2 & 1;
            b2 = (b2 >> 1) | (b << 7);
            b = b3 & 1;
            b3 = (b3 >> 1) | (b4 << 7);
            encryptedCard[num2] = b3;
        }
        encryptedCard[i] = b2;
    }
    if(length == 17) {
        int b2 = encryptedCard[10];
        b2 |= b;
        encryptedCard[10] = b2;
    }
    for(int i = 0; i < length; i++) {
        int j = encryptedCard[i] + (i + 1);
        if(j > 255) {
            j -= 256;
        }
        encryptedCard[i] = c_aEncode[j];
    }
}

uint8_t CalculateCheckSum(uint8_t data[BASIC_ACCESS_BYTE_NUM]) {
    int sum = 0;
    for(int i = 0; i < BASIC_ACCESS_BYTE_NUM - 1; i++) {
        sum += data[i];
    }
    sum = 255 - (sum & 0xFF);
    return sum & 0xFF;
}

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
    basicAccess[16] = CalculateCheckSum(basicAccess);
    EncryptCard(basicAccess, BASIC_ACCESS_BYTE_NUM, buffer);
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
    generate_saflok_key(uid, key);
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
