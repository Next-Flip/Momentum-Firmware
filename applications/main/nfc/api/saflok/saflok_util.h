#pragma once

#include <stdint.h>

#define BASIC_ACCESS_BYTE_NUM 17
#define SAFLOK_YEAR_OFFSET    1980

#ifdef __cplusplus
extern "C" {
#endif

uint8_t saflok_calculate_checksum(uint8_t data[BASIC_ACCESS_BYTE_NUM]);
void saflok_generate_key(const uint8_t* uid, uint8_t* key);
void saflok_decrypt_card(
    uint8_t strCard[BASIC_ACCESS_BYTE_NUM],
    int length,
    uint8_t decryptedCard[BASIC_ACCESS_BYTE_NUM]);
void saflok_encrypt_card(unsigned char* keyCard, int length, unsigned char* encryptedCard);

#ifdef __cplusplus
}
#endif
