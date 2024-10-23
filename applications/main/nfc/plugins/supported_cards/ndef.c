// Parser for NDEF format data
// Supports multiple NDEF messages and records in same tag
// Parsed types: URI (+ Phone, Mail), Text, BT MAC, Contact, WiFi, Empty
// Documentation and sources indicated where relevant
// Made by @Willy-JL
// Mifare Classic support added by @luu176

// We use an arbitrary position system here, in order to support more protocols.
// Each protocol parses basic structure of the card, then starts ndef_parse_tlv()
// using an arbitrary position value that it can understand. When accessing data
// to parse NDEF content, ndef_read() will then map this arbitrary value to the
// card using state in Ndef struct, skip blocks or sectors as needed. This way,
// NDEF parsing code does not need to know details of card layout.

#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#include <bit_lib.h>

#define TAG "NDEF"

void find_ndef_sectors_num(uint8_t block_num, size_t byte_position, int* sec) {
    if(block_num == 1) {
        if(byte_position >= 3 && byte_position <= 16) {
            *sec = (byte_position - 3) / 2 + 1;
        } else {
            *sec = -1;
        }
        return;
    }
    if(block_num == 2) {
        if(byte_position >= 1 && byte_position <= 16) {
            *sec = (byte_position - 1) / 2 + 8;
        } else {
            *sec = -1;
        }
        return;
    }
    *sec = -1;
}

int* find_ndef_locations(const MfClassicData* data, size_t* result_count) {
    *result_count = 0;
    int* results = (int*)malloc(16 * sizeof(int));
    bool found_ndef_aid = false;

    for(uint8_t block_num = 1; block_num <= 2; block_num++) {
        const uint8_t* block_data = data->block[block_num].data;

        for(size_t i = 0; i < 16; i++) {
            if(block_data[i] == 0xE1 && i > 0 && block_data[i - 1] == 0x03) {
                int sec = -1;
                find_ndef_sectors_num(block_num, i, &sec);

                if(sec >= 0) {
                    results[*result_count] = sec;
                    (*result_count)++;
                    found_ndef_aid = true;
                }
            }
        }
    }

    if(!found_ndef_aid) {
        free(results);
        return NULL;
    }

    return results;
}

uint8_t* clone_buffer_without_trailer(const MfClassicData* data) {
    const uint8_t blocks_per_sector = 4;
    uint8_t* new_buffer = malloc(
        blocks_per_sector * (mf_classic_get_total_block_num(data->type) / 4) *
        MF_CLASSIC_BLOCK_SIZE); // allocate new buffer

    uint8_t buffer_idx = 0;

    for(uint8_t sector = 0; sector < (mf_classic_get_total_block_num(data->type) / 4); sector++) {
        for(uint8_t block = 0; block < 3;
            block++) { // Only first 3 blocks, skip the 4th (sector trailer)
            const uint8_t* block_data = data->block[sector * blocks_per_sector + block].data;
            memcpy(
                &new_buffer[buffer_idx * MF_CLASSIC_BLOCK_SIZE],
                block_data,
                MF_CLASSIC_BLOCK_SIZE);
            buffer_idx++;
        }
    }

    return new_buffer;
}

// ---=== structures ===---

// TLV structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/nfc/doc/type_2_tag.html#data
typedef enum FURI_PACKED {
    NdefTlvPadding = 0x00,
    NdefTlvLockControl = 0x01,
    NdefTlvMemoryControl = 0x02,
    NdefTlvNdefMessage = 0x03,
    NdefTlvProprietary = 0xFD,
    NdefTlvTerminator = 0xFE,
} NdefTlv;

// Type Name Format values:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/nfc/index.html#flags-and-tnf
typedef enum FURI_PACKED {
    NdefTnfEmpty = 0x00,
    NdefTnfWellKnownType = 0x01,
    NdefTnfMediaType = 0x02,
    NdefTnfAbsoluteUri = 0x03,
    NdefTnfExternalType = 0x04,
    NdefTnfUnknown = 0x05,
    NdefTnfUnchanged = 0x06,
    NdefTnfReserved = 0x07,
} NdefTnf;

// Flags and TNF structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/nfc/index.html#flags-and-tnf
typedef struct FURI_PACKED {
    // Reversed due to endianness
    NdefTnf type_name_format : 3;
    bool id_length_present   : 1;
    bool short_record        : 1;
    bool chunk_flag          : 1;
    bool message_end         : 1;
    bool message_begin       : 1;
} NdefFlagsTnf;
_Static_assert(sizeof(NdefFlagsTnf) == 1);

// URI payload format:
// https://learn.adafruit.com/adafruit-pn532-rfid-nfc/ndef#uri-records-0x55-slash-u-607763
static const char* ndef_uri_prepends[] = {
    [0x00] = "",
    [0x01] = "http://www.",
    [0x02] = "https://www.",
    [0x03] = "http://",
    [0x04] = "https://",
    [0x05] = "tel:",
    [0x06] = "mailto:",
    [0x07] = "ftp://anonymous:anonymous@",
    [0x08] = "ftp://ftp.",
    [0x09] = "ftps://",
    [0x0A] = "sftp://",
    [0x0B] = "smb://",
    [0x0C] = "nfs://",
    [0x0D] = "ftp://",
    [0x0E] = "dav://",
    [0x0F] = "news:",
    [0x10] = "telnet://",
    [0x11] = "imap:",
    [0x12] = "rtsp://",
    [0x13] = "urn:",
    [0x14] = "pop:",
    [0x15] = "sip:",
    [0x16] = "sips:",
    [0x17] = "tftp:",
    [0x18] = "btspp://",
    [0x19] = "btl2cap://",
    [0x1A] = "btgoep://",
    [0x1B] = "tcpobex://",
    [0x1C] = "irdaobex://",
    [0x1D] = "file://",
    [0x1E] = "urn:epc:id:",
    [0x1F] = "urn:epc:tag:",
    [0x20] = "urn:epc:pat:",
    [0x21] = "urn:epc:raw:",
    [0x22] = "urn:epc:",
    [0x23] = "urn:nfc:",
};

// ---=== card memory layout abstraction ===---

// Shared context and state, read above
typedef struct {
    FuriString* output;
    NfcProtocol protocol;
    union {
        struct {
            const uint8_t* start;
            size_t size;
        } ul;
        struct {
        } mfc;
    };
} Ndef;

static bool ndef_read(Ndef* ndef, size_t pos, size_t len, void* buf) {
    if(ndef->protocol == NfcProtocolMfUltralight) {
        // Memory space is contiguous, simply need to remap to data pointer
        if(pos + len > ndef->ul.size) return false;
        memcpy(buf, ndef->ul.start + pos, len);
        return true;

    } else if(ndef->protocol == NfcProtocolMfClassic) {
        // Need to skip sector trailers
        furi_crash();

    } else {
        furi_crash();
    }
}

// ---=== output helpers ===---

static inline bool is_printable(char c) {
    return (c >= ' ' && c <= '~') || c == '\r' || c == '\n';
}

static bool is_text(const uint8_t* buf, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(!is_printable(buf[i])) return false;
    }
    return true;
}

static bool ndef_dump(Ndef* ndef, const char* prefix, size_t pos, size_t len, bool force_hex) {
    if(prefix) furi_string_cat_printf(ndef->output, "%s: ", prefix);
    // We don't have direct access to memory chunks due to different card layouts
    // Making a temporary buffer is wasteful of RAM and we can't afford this
    // So while iterating like this is inefficient, it saves RAM and works between multiple card types
    if(!force_hex) {
        // If we find a non-printable character along the way, reset string to prev state and re-do as hex
        size_t string_prev = furi_string_size(ndef->output);
        for(size_t i = 0; i < len; i++) {
            char c;
            if(!ndef_read(ndef, pos + i, 1, &c)) return false;
            if(!is_printable(c)) {
                furi_string_left(ndef->output, string_prev);
                force_hex = true;
                break;
            }
            furi_string_push_back(ndef->output, c);
        }
    }
    if(force_hex) {
        for(size_t i = 0; i < len; i++) {
            uint8_t b;
            if(!ndef_read(ndef, pos + i, 1, &b)) return false;
            furi_string_cat_printf(ndef->output, "%02X ", b);
        }
    }
    furi_string_cat(ndef->output, "\n");
    return true;
}

static void
    ndef_print(Ndef* ndef, const char* prefix, const void* buf, size_t len, bool force_hex) {
    if(prefix) furi_string_cat_printf(ndef->output, "%s: ", prefix);
    if(!force_hex && is_text(buf, len)) {
        furi_string_cat_printf(ndef->output, "%.*s", len, (const char*)buf);
    } else {
        for(size_t i = 0; i < len; i++) {
            furi_string_cat_printf(ndef->output, "%02X ", ((const uint8_t*)buf)[i]);
        }
    }
    furi_string_cat(ndef->output, "\n");
}

// ---=== payload parsing ===---

static inline uint8_t hex_to_int(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static char decode_char(const char* str) {
    return (hex_to_int(str[1]) << 4) | hex_to_int(str[2]);
}

static bool ndef_parse_uri(Ndef* ndef, size_t pos, size_t len) {
    const char* prepend = "";
    uint8_t prepend_type;
    if(!ndef_read(ndef, pos, 1, &prepend_type)) return false;
    if(prepend_type < COUNT_OF(ndef_uri_prepends)) {
        prepend = ndef_uri_prepends[prepend_type];
    }
    size_t prepend_len = strlen(prepend);

    size_t uri_len = prepend_len + (len - 1);
    char* const uri_buf = malloc(uri_len); // const to keep the original pointer to free later
    memcpy(uri_buf, prepend, prepend_len);
    if(!ndef_read(ndef, pos + 1, len - 1, uri_buf + prepend_len)) return false;
    char* uri = uri_buf; // cursor we can iterate and shift freely

    // Encoded chars take 3 bytes (%AB), decoded chars take 1 byte
    // We can decode by iterating and overwriting the same buffer
    size_t decoded_len = 0;
    for(size_t encoded_idx = 0; encoded_idx < uri_len; encoded_idx++) {
        if(uri[encoded_idx] == '%' && encoded_idx + 2 < uri_len) {
            char hi = toupper(uri[encoded_idx + 1]);
            char lo = toupper(uri[encoded_idx + 2]);
            if(((hi >= 'A' && hi <= 'F') || (hi >= '0' && hi <= '9')) &&
               ((lo >= 'A' && lo <= 'F') || (lo >= '0' && lo <= '9'))) {
                uri[decoded_len++] = decode_char(&uri[encoded_idx]);
                encoded_idx += 2;
                continue;
            }
        }
        uri[decoded_len++] = uri[encoded_idx];
    }

    const char* type = "URI";
    if(strncmp(uri, "http", 4) == 0) {
        type = "URL";
    } else if(strncmp(uri, "tel:", 4) == 0) {
        type = "Phone";
        uri += 4;
        decoded_len -= 4;
    } else if(strncmp(uri, "mailto:", 7) == 0) {
        type = "Mail";
        uri += 7;
        decoded_len -= 7;
    }

    furi_string_cat_printf(ndef->output, "%s\n", type);
    ndef_print(ndef, NULL, uri, decoded_len, false);

    free(uri_buf);
    return true;
}

static bool ndef_parse_text(Ndef* ndef, size_t pos, size_t len) {
    furi_string_cat(ndef->output, "Text\n");
    if(!ndef_dump(ndef, NULL, pos + 3, len - 3, false)) return false;
    return true;
}

static bool ndef_parse_bt(Ndef* ndef, size_t pos, size_t len) {
    furi_string_cat(ndef->output, "BT MAC\n");
    if(len != 8) return false;
    if(!ndef_dump(ndef, NULL, pos + 2, len - 2, true)) return false;
    return true;
}

static bool ndef_parse_vcard(Ndef* ndef, size_t pos, size_t len) {
    size_t end = pos + len;

    // Same concept as ndef_dump(), inefficient but has least drawbacks
    FuriString* fmt = furi_string_alloc();
    furi_string_reserve(fmt, len + 1);
    while(pos < end) {
        char c;
        if(!ndef_read(ndef, pos++, 1, &c)) return false;
        furi_string_push_back(fmt, c);
    }

    furi_string_trim(fmt);
    if(furi_string_start_with(fmt, "BEGIN:VCARD")) {
        furi_string_right(fmt, furi_string_search_char(fmt, '\n'));
        if(furi_string_end_with(fmt, "END:VCARD")) {
            furi_string_left(fmt, furi_string_search_rchar(fmt, '\n'));
        }
        furi_string_trim(fmt);
        if(furi_string_start_with(fmt, "VERSION:")) {
            furi_string_right(fmt, furi_string_search_char(fmt, '\n'));
            furi_string_trim(fmt);
        }
    }

    furi_string_cat(ndef->output, "Contact\n");
    ndef_print(ndef, NULL, furi_string_get_cstr(fmt), furi_string_size(fmt), false);
    furi_string_free(fmt);
    return true;
}

// Loosely based on Android WiFi NDEF implementation:
// https://android.googlesource.com/platform/packages/apps/Nfc/+/025560080737b43876c9d81feff3151f497947e8/src/com/android/nfc/NfcWifiProtectedSetup.java
static bool ndef_parse_wifi(Ndef* ndef, size_t pos, size_t len) {
#define CREDENTIAL_FIELD_ID        (0x100E)
#define SSID_FIELD_ID              (0x1045)
#define NETWORK_KEY_FIELD_ID       (0x1027)
#define AUTH_TYPE_FIELD_ID         (0x1003)
#define AUTH_TYPE_EXPECTED_SIZE    (2)
#define AUTH_TYPE_OPEN             (0x0001)
#define AUTH_TYPE_WPA_PSK          (0x0002)
#define AUTH_TYPE_WPA_EAP          (0x0008)
#define AUTH_TYPE_WPA2_EAP         (0x0010)
#define AUTH_TYPE_WPA2_PSK         (0x0020)
#define AUTH_TYPE_WPA_AND_WPA2_PSK (0x0022)
#define MAX_NETWORK_KEY_SIZE_BYTES (64)

    furi_string_cat(ndef->output, "WiFi\n");
    size_t end = pos + len;

    uint8_t tmp_buf[2];
    while(pos < len) {
        if(!ndef_read(ndef, pos, 2, &tmp_buf)) return false;
        uint16_t field_id = bit_lib_bytes_to_num_be(tmp_buf, 2);
        pos += 2;
        if(!ndef_read(ndef, pos, 2, &tmp_buf)) return false;
        uint16_t field_len = bit_lib_bytes_to_num_be(tmp_buf, 2);
        pos += 2;

        if(pos + field_len > end) {
            return false;
        }

        if(field_id == CREDENTIAL_FIELD_ID) {
            size_t field_end = pos + field_len;
            while(pos < field_end) {
                if(!ndef_read(ndef, pos, 2, &tmp_buf)) return false;
                uint16_t cfg_id = bit_lib_bytes_to_num_be(tmp_buf, 2);
                pos += 2;
                if(!ndef_read(ndef, pos, 2, &tmp_buf)) return false;
                uint16_t cfg_len = bit_lib_bytes_to_num_be(tmp_buf, 2);
                pos += 2;

                if(pos + cfg_len > field_end) {
                    return false;
                }

                switch(cfg_id) {
                case SSID_FIELD_ID:
                    if(!ndef_dump(ndef, "SSID", pos, cfg_len, false)) return false;
                    pos += cfg_len;
                    break;
                case NETWORK_KEY_FIELD_ID:
                    if(cfg_len > MAX_NETWORK_KEY_SIZE_BYTES) {
                        return false;
                    }
                    if(!ndef_dump(ndef, "PWD", pos, cfg_len, false)) return false;
                    pos += cfg_len;
                    break;
                case AUTH_TYPE_FIELD_ID:
                    if(cfg_len != AUTH_TYPE_EXPECTED_SIZE) {
                        return false;
                    }
                    if(!ndef_read(ndef, pos, 2, &tmp_buf)) return false;
                    uint16_t auth_type = bit_lib_bytes_to_num_be(tmp_buf, 2);
                    pos += 2;
                    const char* auth;
                    switch(auth_type) {
                    case AUTH_TYPE_OPEN:
                        auth = "Open";
                        break;
                    case AUTH_TYPE_WPA_PSK:
                        auth = "WPA Personal";
                        break;
                    case AUTH_TYPE_WPA_EAP:
                        auth = "WPA Enterprise";
                        break;
                    case AUTH_TYPE_WPA2_EAP:
                        auth = "WPA2 Enterprise";
                        break;
                    case AUTH_TYPE_WPA2_PSK:
                        auth = "WPA2 Personal";
                        break;
                    case AUTH_TYPE_WPA_AND_WPA2_PSK:
                        auth = "WPA/WPA2 Personal";
                        break;
                    default:
                        auth = "Unknown";
                        break;
                    }
                    ndef_print(ndef, "AUTH", auth, strlen(auth), false);
                    break;
                default:
                    pos += cfg_len;
                    break;
                }
            }
            return true;
        }
        pos += field_len;
    }

    furi_string_cat(ndef->output, "No data parsed\n");
    return true;
}

static bool ndef_parse_payload(
    Ndef* ndef,
    size_t pos,
    size_t len,
    NdefTnf tnf,
    const char* type,
    uint8_t type_len) {
    if(!len) {
        furi_string_cat(ndef->output, "Empty\n");
        return true;
    }

    switch(tnf) {
    case NdefTnfWellKnownType:
        if(strncmp("U", type, type_len) == 0) {
            return ndef_parse_uri(ndef, pos, len);
        } else if(strncmp("T", type, type_len) == 0) {
            return ndef_parse_text(ndef, pos, len);
        }
        ndef_print(ndef, "Well-known type", type, type_len, false);
        if(!ndef_dump(ndef, "Payload", pos, len, false)) return false;
        return true;

    case NdefTnfMediaType:
        if(strncmp("application/vnd.bluetooth.ep.oob", type, type_len) == 0) {
            return ndef_parse_bt(ndef, pos, len);
        } else if(strncmp("text/vcard", type, type_len) == 0) {
            return ndef_parse_vcard(ndef, pos, len);
        } else if(strncmp("application/vnd.wfa.wsc", type, type_len) == 0) {
            return ndef_parse_wifi(ndef, pos, len);
        }
        ndef_print(ndef, "Media Type", type, type_len, false);
        if(!ndef_dump(ndef, "Payload", pos, len, false)) return false;
        return true;

    case NdefTnfEmpty:
    case NdefTnfAbsoluteUri:
    case NdefTnfExternalType:
    case NdefTnfUnknown:
    case NdefTnfUnchanged:
    case NdefTnfReserved:
    default:
        // Dump data without parsing
        ndef_print(ndef, "Type name format", &tnf, 1, true);
        ndef_print(ndef, "Type", type, type_len, false);
        if(!ndef_dump(ndef, "Payload", pos, len, false)) return false;
        return true;
    }
}

// ---=== tlv and message parsing ===---

// NDEF message structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/nfc/index.html#ndef_message_and_record_format
static bool ndef_parse_message(Ndef* ndef, size_t pos, size_t len, size_t message_num) {
    size_t end = pos + len;

    size_t record_num = 0;
    bool last_record = false;
    while(pos < end) {
        // Flags and TNF
        NdefFlagsTnf flags_tnf;
        if(!ndef_read(ndef, pos++, 1, &flags_tnf)) return false;
        FURI_LOG_D(TAG, "flags_tnf: %02X", *(uint8_t*)&flags_tnf);
        FURI_LOG_D(TAG, "flags_tnf.message_begin: %d", flags_tnf.message_begin);
        FURI_LOG_D(TAG, "flags_tnf.message_end: %d", flags_tnf.message_end);
        FURI_LOG_D(TAG, "flags_tnf.chunk_flag: %d", flags_tnf.chunk_flag);
        FURI_LOG_D(TAG, "flags_tnf.short_record: %d", flags_tnf.short_record);
        FURI_LOG_D(TAG, "flags_tnf.id_length_present: %d", flags_tnf.id_length_present);
        FURI_LOG_D(TAG, "flags_tnf.type_name_format: %02X", flags_tnf.type_name_format);
        // Message Begin should only be set on first record
        if(record_num++ && flags_tnf.message_begin) return false;
        // Message End should only be set on last record
        if(last_record) return false;
        if(flags_tnf.message_end) last_record = true;
        // Chunk Flag not supported
        if(flags_tnf.chunk_flag) return false;

        // Type Length
        uint8_t type_len;
        if(!ndef_read(ndef, pos++, 1, &type_len)) return false;

        // Payload Length field of 1 or 4 bytes
        uint32_t payload_len;
        if(flags_tnf.short_record) {
            uint8_t payload_len_short;
            if(!ndef_read(ndef, pos++, 1, &payload_len_short)) return false;
            payload_len = payload_len_short;
        } else {
            if(!ndef_read(ndef, pos, 4, &payload_len)) return false;
            FURI_LOG_I(TAG, "raw %ld", payload_len);
            uint8_t payload_len_buf[4]; // FIXME: might not need a buffer, just copy to uint32_t
            if(!ndef_read(ndef, pos, 4, payload_len_buf)) return false;
            payload_len = bit_lib_bytes_to_num_be(payload_len_buf, 4);
            FURI_LOG_I(TAG, "expected %ld", payload_len);
            pos += 4;
        }

        // ID Length
        uint8_t id_len = 0;
        if(flags_tnf.id_length_present) {
            if(!ndef_read(ndef, pos++, 1, &id_len)) return false;
        }

        // Payload Type
        char* type = NULL;
        if(type_len) {
            type = malloc(type_len);
            if(!ndef_read(ndef, pos, type_len, type)) {
                free(type);
                return false;
            }
            pos += type_len;
        }

        // Payload ID
        pos += id_len;

        furi_string_cat_printf(ndef->output, "\e*> M:%d R:%d - ", message_num, record_num);
        if(!ndef_parse_payload(
               ndef, pos, payload_len, flags_tnf.type_name_format, type, type_len)) {
            free(type);
            return false;
        }
        pos += payload_len;

        free(type);
        furi_string_trim(ndef->output, "\n");
        furi_string_cat(ndef->output, "\n\n");
    }

    return pos == end && last_record;
}

// TLV structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/nfc/doc/type_2_tag.html#data
static bool ndef_parse_tlv(Ndef* ndef, size_t pos) {
    size_t message_num = 0;

    while(true) {
        NdefTlv tlv;
        if(!ndef_read(ndef, pos++, 1, &tlv)) return false;
        FURI_LOG_D(TAG, "tlv: %02X", tlv);

        switch(tlv) {
        default:
            // Unknown, bail to avoid problems
            return false;

        case NdefTlvPadding:
            // Has no length, skip to next byte
            break;

        case NdefTlvTerminator:
            // NDEF message finished, return whether we parsed something
            return message_num != 0;

        case NdefTlvLockControl:
        case NdefTlvMemoryControl:
        case NdefTlvProprietary:
        case NdefTlvNdefMessage: {
            // Calculate length
            uint16_t len;
            uint8_t len_type;
            if(!ndef_read(ndef, pos++, 1, &len_type)) return false;
            if(len_type < 0xFF) { // 1 byte length
                len = len_type;
            } else { // 3 byte length (0xFF marker + 2 byte integer)
                if(!ndef_read(ndef, pos, 2, &len)) return false;
                FURI_LOG_I(TAG, "raw %d", len);
                uint8_t len_buf[2]; // FIXME: might not need a buffer, just copy to uint16_t
                if(!ndef_read(ndef, pos, 2, len_buf)) return false;
                len = bit_lib_bytes_to_num_be(len_buf, 2);
                FURI_LOG_I(TAG, "expected %d", len);
                pos += 2;
            }

            if(tlv != NdefTlvNdefMessage) {
                // We don't care, skip this TLV block to next one
                pos += len;
                break;
            }

            if(!ndef_parse_message(ndef, pos, len, ++message_num)) return false;
            pos += len;

            break;
        }
        }
    }
}

// ---=== protocol entry-points ===---

// MF UL memory layout:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/nfc/doc/type_2_tag.html#memory_layout
static bool ndef_ul_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);

    // Check card type can contain NDEF
    if(data->type != MfUltralightTypeNTAG203 && data->type != MfUltralightTypeNTAG213 &&
       data->type != MfUltralightTypeNTAG215 && data->type != MfUltralightTypeNTAG216 &&
       data->type != MfUltralightTypeNTAGI2C1K && data->type != MfUltralightTypeNTAGI2C2K &&
       data->type != MfUltralightTypeNTAGI2CPlus1K &&
       data->type != MfUltralightTypeNTAGI2CPlus2K) {
        return false;
    }

    // Double check Capability Container (CC) values
    struct {
        uint8_t nfc_magic_number;
        uint8_t document_version_number;
        uint8_t data_area_size;
        uint8_t read_write_access;
    }* cc = (void*)&data->page[3].data[0];
    if(cc->nfc_magic_number != 0xE1) return false;
    if(cc->document_version_number != 0x10) return false;

    // Calculate usable data area
    const uint8_t* start = &data->page[4].data[0];
    const uint8_t* end = start + (cc->data_area_size * 2 * MF_ULTRALIGHT_PAGE_SIZE);
    size_t max_size = mf_ultralight_get_pages_total(data->type) * MF_ULTRALIGHT_PAGE_SIZE;
    end = MIN(end, &data->page[0].data[0] + max_size);

    furi_string_printf(
        parsed_data,
        "\e#NDEF Format Data\nCard type: %s\n",
        mf_ultralight_get_device_name(data, NfcDeviceNameTypeFull));

    Ndef ndef = {
        .output = parsed_data,
        .protocol = NfcProtocolMfUltralight,
        .ul =
            {
                .start = start,
                .size = end - start,
            },
    };
    bool parsed = ndef_parse_tlv(&ndef, 0);

    if(parsed) {
        furi_string_trim(parsed_data, "\n");
        furi_string_cat(parsed_data, "\n");
    } else {
        furi_string_reset(parsed_data);
    }

    return parsed;
}

static bool ndef_mfc_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
    uint8_t* cleaned_buffer =
        clone_buffer_without_trailer(data); /* This is very important, it removes
    all sector trailers from the data buffer to avoid ndef parsing to go into the sector trailer
    */
    bool parsed = false;
    if(data->type != MfClassicType1k && data->type != MfClassicType4k &&
       data->type != MfClassicTypeMini) {
        return false;
    }

    size_t result_count;
    int* results = find_ndef_locations(data, &result_count);
    if(!results) {
        return false;
    }

    // number of valid ndef apps
    size_t ndef_sectors_num_to_check = 0;
    for(size_t i = 0; i < result_count; i++) {
        const size_t ndef_start_block_num = results[i] * 3;
        const size_t start_offset = ndef_start_block_num * MF_CLASSIC_BLOCK_SIZE;
        const uint8_t* block_data = &cleaned_buffer[start_offset];

        for(size_t j = 0; j < MF_CLASSIC_BLOCK_SIZE; j++) {
            if(block_data[j] == 0x03) {
                ndef_sectors_num_to_check++;
                break;
            }
        }
    }

    if(ndef_sectors_num_to_check > 0) {
        furi_string_printf(
            parsed_data,
            "\e#NDEF Format Data\nCard type: %s\n",
            mf_classic_get_device_name(data, NfcDeviceNameTypeFull));
    }
    size_t cur_sector = 0;
    for(size_t i = 0; i < result_count; i++) {
        const uint8_t ndef_start_block_num = results[i] * 3;
        const size_t start_offset = ndef_start_block_num * MF_CLASSIC_BLOCK_SIZE;
        const uint8_t* block_data = &cleaned_buffer[start_offset];

        const uint8_t* tlv_pos = NULL;
        for(size_t j = 0; j < MF_CLASSIC_BLOCK_SIZE; j++) {
            if(block_data[j] == 0x03) {
                tlv_pos = &block_data[j];
                break;
            }
        }

        if(tlv_pos) {
            cur_sector++;
            const uint8_t* data_area_size_ptr = tlv_pos + 4;
            uint8_t data_area_size = *data_area_size_ptr;
            const uint8_t* cur = tlv_pos;
            size_t ndef_length = data_area_size * MF_CLASSIC_BLOCK_SIZE;
            const uint8_t* end = cur + ndef_length - 1;
            size_t max_size = mf_classic_get_total_block_num(data->type) * MF_CLASSIC_BLOCK_SIZE;
            end = MIN(end, &cleaned_buffer[0] + max_size);

            while(cur < end) {
                switch(*cur++) {
                case 0x03: { // NDEF message
                    if(cur >= end) break;

                    uint16_t len;
                    if(*cur < 0xFF) { // 1 byte length
                        len = *cur++;
                    } else { // 3 byte length (0xFF marker + 2 byte integer)
                        if(cur + 2 >= end) {
                            cur = end;
                            break;
                        }
                        len = bit_lib_bytes_to_num_be(++cur, 2);
                        cur += 2;
                    }

                    if(cur + len >= end) {
                        cur = end;
                        break;
                    }

                    const uint8_t* message_end = cur + len;
                    cur = parse_ndef_message(parsed_data, cur_sector, cur, message_end);
                    if(cur != message_end) cur = end;
                    ndef_sectors_num_to_check--; // subtract by 1 when finished parsing sector x
                    break;
                }

                case 0xFE: // TLV end
                    cur = end;
                    if(ndef_sectors_num_to_check == 1 || ndef_sectors_num_to_check == 0)
                        parsed = true;
                    break;

                case 0x00: // Padding, has no length, skip
                    break;

                case 0x01: // Lock control
                case 0x02: // Memory control
                case 0xFD: // Proprietary
                    // We don't care, skip this TLV block
                    if(cur >= end) break;
                    if(*cur < 0xFF) { // 1 byte length
                        cur += *cur + 1; // Shift by TLV length
                    } else { // 3 byte length (0xFF marker + 2 byte integer)
                        if(cur + 2 >= end) {
                            cur = end;
                            break;
                        }
                        cur += bit_lib_bytes_to_num_be(cur + 1, 2) + 3; // Shift by TLV length
                    }
                    break;

                default: // Unknown, bail to avoid problems
                    cur = end;
                    break;
                }
            }

            if(parsed) {
                furi_string_trim(parsed_data, "\n");
                furi_string_cat(parsed_data, "\n");
            } else {
                furi_string_reset(parsed_data);
            }
        }
    }

    free(results); // Free the memory allocated for results
    return parsed;
}

// ---=== boilerplate ===---

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin ndef_ul_plugin = {
    .protocol = NfcProtocolMfUltralight,
    .verify = NULL,
    .read = NULL,
    .parse = ndef_ul_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor ndef_ul_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ndef_ul_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* ndef_ul_plugin_ep(void) {
    return &ndef_ul_plugin_descriptor;
}

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin ndef_mfc_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = NULL,
    .read = NULL,
    .parse = ndef_mfc_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor ndef_mfc_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ndef_mfc_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* ndef_mfc_plugin_ep(void) {
    return &ndef_mfc_plugin_descriptor;
}
