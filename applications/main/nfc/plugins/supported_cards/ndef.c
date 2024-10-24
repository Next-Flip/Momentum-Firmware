// Parser for NDEF format data
// Supports multiple NDEF messages and records in same tag
// Parsed types: URI (+ Phone, Mail), Text, BT MAC, Contact, WiFi, Empty
// Documentation and sources indicated where relevant
// Made by @Willy-JL
// Mifare Classic support added by @luu176

// We use an arbitrary position system here, in order to support more protocols.
// Each protocol parses basic structure of the card, then starts ndef_parse_tlv()
// using an arbitrary position value that it can understand. When accessing data
// to parse NDEF content, ndef_get() will then map this arbitrary value to the
// card using state in Ndef struct, skip blocks or sectors as needed. This way,
// NDEF parsing code does not need to know details of card layout.

#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#include <bit_lib.h>

#define TAG "NDEF"

#define NDEF_PROTO_NONE (0)
#define NDEF_PROTO_UL   (1)
#define NDEF_PROTO_MFC  (2)

#if !defined(NDEF_PROTO) || (NDEF_PROTO != NDEF_PROTO_UL && NDEF_PROTO != NDEF_PROTO_MFC)
#error Must specify what protocol to use with NDEF_PROTO define!
#endif

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
    // clang-format off
    [0x00] = NULL,
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
    // clang-format on
};

// ---=== card memory layout abstraction ===---

// Shared context and state, read above
typedef struct {
    FuriString* output;
#if NDEF_PROTO == NDEF_PROTO_UL
    struct {
        const uint8_t* start;
        size_t size;
    } ul;
#elif NDEF_PROTO == NDEF_PROTO_MFC
    struct {
        const MfClassicBlock* blocks;
        size_t size;
    } mfc;
#endif
} Ndef;

static bool ndef_get(Ndef* ndef, size_t pos, size_t len, void* buf) {
#if NDEF_PROTO == NDEF_PROTO_UL

    // Memory space is contiguous, simply need to remap to data pointer
    if(pos + len > ndef->ul.size) return false;
    memcpy(buf, ndef->ul.start + pos, len);
    return true;

#elif NDEF_PROTO == NDEF_PROTO_MFC

    // We need to skip sector trailers and MAD2, NDEF parsing just uses
    // a position offset in data space, as if it were contiguous.

    // Start with a simple data space size check
    if(pos + len > ndef->mfc.size) return false;

    // First 128 blocks are 32 sectors: 3 data blocks, 1 sector trailer.
    // Sector 16 contains MAD2 and we need to skip this.
    // So the first 93 (31*3) data blocks correspond to 128 real blocks.
    // Last 128 blocks are 8 sectors: 15 data blocks, 1 sector trailer.
    // So the last 120 (8*15) data blocks correspond to 128 real blocks.
    div_t small_sector_data_blocks = div(pos, MF_CLASSIC_BLOCK_SIZE);
    size_t large_sector_data_blocks = 0;
    if(small_sector_data_blocks.quot > 93) {
        large_sector_data_blocks = small_sector_data_blocks.quot - 93;
        small_sector_data_blocks.quot = 93;
    }

    div_t small_sectors = div(small_sector_data_blocks.quot, 3);
    size_t real_block = small_sectors.quot * 4 + small_sectors.rem;
    if(small_sectors.quot >= 16) {
        real_block += 4; // Skip MAD2
    }
    if(large_sector_data_blocks) {
        div_t large_sectors = div(large_sector_data_blocks, 15);
        real_block += large_sectors.quot * 16 + large_sectors.rem;
    }

    const uint8_t* cur = &ndef->mfc.blocks[real_block].data[small_sector_data_blocks.rem];
    while(len) {
        size_t sector_trailer = mf_classic_get_sector_trailer_num_by_block(real_block);
        const uint8_t* end = &ndef->mfc.blocks[sector_trailer].data[0];

        size_t chunk_len = MIN((size_t)(end - cur), len);
        memcpy(buf, cur, chunk_len);
        len -= chunk_len;

        if(len) {
            real_block = sector_trailer + 1;
            if(real_block == 64) {
                real_block += 4; // Skip MAD2
            }
            cur = &ndef->mfc.blocks[real_block].data[0];
        }
    }

    return true;

#else

    UNUSED(ndef);
    UNUSED(pos);
    UNUSED(len);
    UNUSED(buf);
    return false;

#endif
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
            if(!ndef_get(ndef, pos + i, 1, &c)) return false;
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
            if(!ndef_get(ndef, pos + i, 1, &b)) return false;
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

static char url_decode_char(const char* str) {
    return (hex_to_int(str[0]) << 4) | hex_to_int(str[1]);
}

static bool ndef_parse_uri(Ndef* ndef, size_t pos, size_t len) {
    const char* type = "URI";

    // Parse URI prepend type
    const char* prepend = NULL;
    uint8_t prepend_type;
    if(!ndef_get(ndef, pos++, 1, &prepend_type)) return false;
    len--;
    if(prepend_type < COUNT_OF(ndef_uri_prepends)) {
        prepend = ndef_uri_prepends[prepend_type];
    }
    if(prepend) {
        if(strncmp(prepend, "http", 4) == 0) {
            type = "URL";
        } else if(strncmp(prepend, "tel:", 4) == 0) {
            type = "Phone";
            prepend = ""; // Not NULL to avoid schema check below, only want to hide it from output
        } else if(strncmp(prepend, "mailto:", 7) == 0) {
            type = "Mail";
            prepend = ""; // Not NULL to avoid schema check below, only want to hide it from output
        }
    }

    // Parse and optionally skip schema, if no prepend was specified
    if(!prepend) {
        char schema[7] = {0}; // Longest schema we check is 7 char long without terminator
        if(!ndef_get(ndef, pos, MIN(sizeof(schema), len), schema)) return false;
        if(strncmp(schema, "http", 4) == 0) {
            type = "URL";
        } else if(strncmp(schema, "tel:", 4) == 0) {
            type = "Phone";
            pos += 4;
            len -= 4;
        } else if(strncmp(schema, "mailto:", 7) == 0) {
            type = "Mail";
            pos += 7;
            len -= 7;
        }
    }

    // Print static data as-is
    furi_string_cat_printf(ndef->output, "%s\n", type);
    if(prepend) {
        furi_string_cat(ndef->output, prepend);
    }

    // Print URI one char at a time and perform URL decode
    while(len) {
        char c;
        if(!ndef_get(ndef, pos++, 1, &c)) return false;
        len--;
        if(c != '%' || len < 2) { // Not encoded, or not enough remaining text for encoded char
            furi_string_push_back(ndef->output, c);
            continue;
        }
        char enc[2];
        if(!ndef_get(ndef, pos, 2, enc)) return false;
        enc[0] = toupper(enc[0]);
        enc[1] = toupper(enc[1]);
        // Only consume and print these 2 characters if they're valid URL encoded
        // Otherwise they're processed in next iterations and we output the % char
        if(((enc[0] >= 'A' && enc[0] <= 'F') || (enc[0] >= '0' && enc[0] <= '9')) &&
           ((enc[1] >= 'A' && enc[1] <= 'F') || (enc[1] >= '0' && enc[1] <= '9'))) {
            pos += 2;
            len -= 2;
            c = url_decode_char(enc);
        }
        furi_string_push_back(ndef->output, c);
    }

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
        if(!ndef_get(ndef, pos++, 1, &c)) return false;
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
    while(pos < end) {
        if(!ndef_get(ndef, pos, 2, &tmp_buf)) return false;
        uint16_t field_id = bit_lib_bytes_to_num_be(tmp_buf, 2);
        pos += 2;
        if(!ndef_get(ndef, pos, 2, &tmp_buf)) return false;
        uint16_t field_len = bit_lib_bytes_to_num_be(tmp_buf, 2);
        pos += 2;
        FURI_LOG_D(TAG, "wifi field: %04X len: %d", field_id, field_len);

        if(pos + field_len > end) {
            return false;
        }

        if(field_id == CREDENTIAL_FIELD_ID) {
            size_t field_end = pos + field_len;
            while(pos < field_end) {
                if(!ndef_get(ndef, pos, 2, &tmp_buf)) return false;
                uint16_t cfg_id = bit_lib_bytes_to_num_be(tmp_buf, 2);
                pos += 2;
                if(!ndef_get(ndef, pos, 2, &tmp_buf)) return false;
                uint16_t cfg_len = bit_lib_bytes_to_num_be(tmp_buf, 2);
                pos += 2;
                FURI_LOG_D(TAG, "wifi cfg: %04X len: %d", cfg_id, cfg_len);

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
                    if(!ndef_get(ndef, pos, 2, &tmp_buf)) return false;
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

// ---=== ndef layout parsing ===---

static bool
    ndef_parse_message(Ndef* ndef, size_t pos, size_t len, size_t message_num, bool smart_poster);
static size_t ndef_parse_tlv(Ndef* ndef, size_t pos, size_t already_parsed);
static bool ndef_parse_record(
    Ndef* ndef,
    size_t pos,
    size_t len,
    NdefTnf tnf,
    const char* type,
    uint8_t type_len);

static bool ndef_parse_record(
    Ndef* ndef,
    size_t pos,
    size_t len,
    NdefTnf tnf,
    const char* type,
    uint8_t type_len) {
    FURI_LOG_D(TAG, "payload type: %.*s len: %d", type_len, type, len);
    if(!len) {
        furi_string_cat(ndef->output, "Empty\n");
        return true;
    }

    switch(tnf) {
    case NdefTnfWellKnownType:
        if(strncmp("Sp", type, type_len) == 0) {
            furi_string_cat(ndef->output, "SmartPoster\n");
            return ndef_parse_message(ndef, pos, len, 0, true);
        } else if(strncmp("U", type, type_len) == 0) {
            return ndef_parse_uri(ndef, pos, len);
        } else if(strncmp("T", type, type_len) == 0) {
            return ndef_parse_text(ndef, pos, len);
        }
        // Dump data without parsing
        furi_string_cat(ndef->output, "Unsupported\n");
        ndef_print(ndef, "Well-known Type", type, type_len, false);
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
        // Dump data without parsing
        furi_string_cat(ndef->output, "Unsupported\n");
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
        furi_string_cat(ndef->output, "Unsupported\n");
        ndef_print(ndef, "Type name format", &tnf, 1, true);
        ndef_print(ndef, "Type", type, type_len, false);
        if(!ndef_dump(ndef, "Payload", pos, len, false)) return false;
        return true;
    }
}

// NDEF message structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/nfc/index.html#ndef_message_and_record_format
static bool
    ndef_parse_message(Ndef* ndef, size_t pos, size_t len, size_t message_num, bool smart_poster) {
    size_t end = pos + len;

    size_t record_num = 0;
    bool last_record = false;
    while(pos < end) {
        // Flags and TNF
        NdefFlagsTnf flags_tnf;
        if(!ndef_get(ndef, pos++, 1, &flags_tnf)) return false;
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
        if(!ndef_get(ndef, pos++, 1, &type_len)) return false;

        // Payload Length field of 1 or 4 bytes
        uint32_t payload_len;
        if(flags_tnf.short_record) {
            uint8_t payload_len_short;
            if(!ndef_get(ndef, pos++, 1, &payload_len_short)) return false;
            payload_len = payload_len_short;
        } else {
            if(!ndef_get(ndef, pos, sizeof(payload_len), &payload_len)) return false;
            payload_len = bit_lib_bytes_to_num_be((void*)&payload_len, sizeof(payload_len));
            pos += sizeof(payload_len);
        }

        // ID Length
        uint8_t id_len = 0;
        if(flags_tnf.id_length_present) {
            if(!ndef_get(ndef, pos++, 1, &id_len)) return false;
        }

        // Payload Type
        char type_buf[32]; // Longest type supported in ndef_parse_record() is 32 chars excl terminator
        char* type = type_buf;
        bool type_was_allocated = false;
        if(type_len) {
            if(type_len > sizeof(type_buf)) {
                type = malloc(type_len);
                type_was_allocated = true;
            }
            if(!ndef_get(ndef, pos, type_len, type)) {
                if(type_was_allocated) free(type);
                return false;
            }
            pos += type_len;
        }

        // Payload ID
        pos += id_len;

        if(smart_poster) {
            furi_string_cat_printf(ndef->output, "\e*> SP%d: ", record_num);
        } else {
            furi_string_cat_printf(ndef->output, "\e*> M%dR%d: ", message_num, record_num);
        }
        if(!ndef_parse_record(ndef, pos, payload_len, flags_tnf.type_name_format, type, type_len)) {
            if(type_was_allocated) free(type);
            return false;
        }
        pos += payload_len;

        if(type_was_allocated) free(type);
        furi_string_trim(ndef->output, "\n");
        furi_string_cat(ndef->output, "\n\n");
    }

    return pos == end && last_record;
}

// TLV structure:
// https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/nfc/doc/type_2_tag.html#data
static size_t ndef_parse_tlv(Ndef* ndef, size_t pos, size_t already_parsed) {
    size_t message_num = 0;

    while(true) {
        NdefTlv tlv;
        if(!ndef_get(ndef, pos++, 1, &tlv)) return 0;
        FURI_LOG_D(TAG, "tlv: %02X", tlv);

        switch(tlv) {
        default:
            // Unknown, bail to avoid problems
            return 0;

        case NdefTlvPadding:
            // Has no length, skip to next byte
            break;

        case NdefTlvTerminator:
            // NDEF message finished, return whether we parsed something
            return message_num;

        case NdefTlvLockControl:
        case NdefTlvMemoryControl:
        case NdefTlvProprietary:
        case NdefTlvNdefMessage: {
            // Calculate length
            uint16_t len;
            uint8_t len_type;
            if(!ndef_get(ndef, pos++, 1, &len_type)) return 0;
            if(len_type < 0xFF) { // 1 byte length
                len = len_type;
            } else { // 3 byte length (0xFF marker + 2 byte integer)
                if(!ndef_get(ndef, pos, sizeof(len), &len)) return 0;
                len = bit_lib_bytes_to_num_be((void*)&len, sizeof(len));
                pos += sizeof(len);
            }

            if(tlv != NdefTlvNdefMessage) {
                // We don't care, skip this TLV block to next one
                pos += len;
                break;
            }

            if(!ndef_parse_message(ndef, pos, len, ++message_num + already_parsed, false))
                return 0;
            pos += len;

            break;
        }
        }
    }
}

// ---=== protocol entry-points ===---

#if NDEF_PROTO == NDEF_PROTO_UL

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
        .ul =
            {
                .start = start,
                .size = end - start,
            },
    };
    size_t parsed = ndef_parse_tlv(&ndef, 0, 0);

    if(parsed) {
        furi_string_trim(parsed_data, "\n");
        furi_string_cat(parsed_data, "\n");
    } else {
        furi_string_reset(parsed_data);
    }

    return parsed > 0;
}

#elif NDEF_PROTO == NDEF_PROTO_MFC

// MFC MAD datasheet:
// https://www.nxp.com/docs/en/application-note/AN10787.pdf
#define AID_SIZE (2)
static const uint64_t mad_key = 0xA0A1A2A3A4A5;

// NDEF on MFC breakdown:
// https://learn.adafruit.com/adafruit-pn532-rfid-nfc/ndef#storing-ndef-messages-in-mifare-sectors-607778
static const uint8_t ndef_aid[AID_SIZE] = {0x03, 0xE1};

static bool ndef_mfc_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    // Check card type can contain NDEF
    if(data->type != MfClassicType1k && data->type != MfClassicType4k &&
       data->type != MfClassicTypeMini) {
        return false;
    }

    // Check MADs for what sectors contain NDEF data AIDs
    bool sectors_with_ndef[MF_CLASSIC_TOTAL_SECTORS_MAX] = {0};
    const size_t sector_count = mf_classic_get_total_sectors_num(data->type);
    const struct {
        size_t block;
        uint8_t aid_count;
    } mads[2] = {
        {1, 15},
        {64, 23},
    };
    for(uint8_t mad = 0; mad < COUNT_OF(mads); mad++) {
        if(sector_count <= 16 && mad > 0) break; // Skip MAD2 if not present
        for(uint8_t aid_index = 0; aid_index < mads[mad].aid_count; aid_index++) {
            const size_t block = mads[mad].block;
            const size_t sector = mf_classic_get_sector_by_block(block);
            // Check MAD key
            const MfClassicSectorTrailer* sector_trailer =
                mf_classic_get_sector_trailer_by_sector(data, sector);
            const uint64_t sector_key_a = bit_lib_bytes_to_num_be(
                sector_trailer->key_a.data, COUNT_OF(sector_trailer->key_a.data));
            if(sector_key_a != mad_key) return false;
            // Find NDEF AIDs
            const uint8_t* aid = &data->block[block].data[2 + aid_index * AID_SIZE];
            if(!memcmp(aid, ndef_aid, AID_SIZE)) {
                sectors_with_ndef[aid_index + 1] = true;
            }
        }
    }

    furi_string_printf(
        parsed_data,
        "\e#NDEF Format Data\nCard type: %s\n",
        mf_classic_get_device_name(data, NfcDeviceNameTypeFull));

    // Calculate how large the data space is, so excluding sector trailers and MAD2.
    // Makes sure we stay within this card's actual content when parsing.
    // First 32 sectors: 3 data blocks, 1 sector trailer.
    // Sector 16 contains MAD2 and we need to skip this.
    // So the first 32 sectors correspond to 93 (31*3) data blocks.
    // Last 8 sectors: 15 data blocks, 1 sector trailer.
    // So the last 8 sectors correspond to 120 (8*15) data blocks.
    size_t data_size;
    if(sector_count > 32) {
        data_size = 93 + (sector_count - 32) * 15;
    } else {
        data_size = sector_count * 3;
        if(sector_count >= 16) {
            data_size -= 3; // Skip MAD2
        }
    }
    data_size *= MF_CLASSIC_BLOCK_SIZE;

    Ndef ndef = {
        .output = parsed_data,
        .mfc =
            {
                .blocks = data->block,
                .size = data_size,
            },
    };
    size_t total_parsed = 0;

    for(size_t sector = 0; sector < sector_count; sector++) {
        if(!sectors_with_ndef[sector]) continue;
        FURI_LOG_D(TAG, "sector: %d", sector);
        size_t string_prev = furi_string_size(parsed_data);

        // Convert real sector number to data block number
        // to skip sector trailers and MAD2
        size_t data_block;
        if(sector < 32) {
            data_block = sector * 3;
            if(sector >= 16) {
                data_block -= 3; // Skip MAD2
            }
        } else {
            data_block = 93 + (sector - 32) * 15;
        }
        FURI_LOG_D(TAG, "data_block: %d", data_block);
        size_t parsed = ndef_parse_tlv(&ndef, data_block * MF_CLASSIC_BLOCK_SIZE, total_parsed);

        if(parsed) {
            total_parsed += parsed;
            furi_string_trim(parsed_data, "\n");
            furi_string_cat(parsed_data, "\n");
        } else {
            furi_string_left(parsed_data, string_prev);
        }
    }

    if(!total_parsed) {
        furi_string_reset(parsed_data);
    }

    return total_parsed > 0;
}

#endif

// ---=== boilerplate ===---

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin ndef_plugin = {
    .verify = NULL,
    .read = NULL,
#if NDEF_PROTO == NDEF_PROTO_UL
    .parse = ndef_ul_parse,
    .protocol = NfcProtocolMfUltralight,
#elif NDEF_PROTO == NDEF_PROTO_MFC
    .parse = ndef_mfc_parse,
    .protocol = NfcProtocolMfClassic,
#endif
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor ndef_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ndef_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* ndef_plugin_ep(void) {
    return &ndef_plugin_descriptor;
}
