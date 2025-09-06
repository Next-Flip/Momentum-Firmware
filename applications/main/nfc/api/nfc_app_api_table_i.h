#include "gallagher/gallagher_util.h"
#include "mosgortrans/mosgortrans_util.h"
#include "saflok/saflok_util.h"
#include "../nfc_app_i.h"
#include "../helpers/protocol_support/nfc_protocol_support_gui_common.h"
#include "../helpers/protocol_support/nfc_protocol_support_unlock_helper.h"

/* 
 * A list of app's private functions and objects to expose for plugins.
 * It is used to generate a table of symbols for import resolver to use.
 * TBD: automatically generate this table from app's header files
 */
static constexpr auto nfc_app_api_table = sort(create_array_t<sym_entry>(
    API_METHOD(
        gallagher_deobfuscate_and_parse_credential,
        void,
        (GallagherCredential * credential, const uint8_t* cardholder_data_obfuscated)),
    API_VARIABLE(GALLAGHER_CARDAX_ASCII, const uint8_t*),
    API_METHOD(
        mosgortrans_parse_transport_block,
        bool,
        (const MfClassicBlock* block, FuriString* result)),
    API_METHOD(
        render_section_header,
        void,
        (FuriString * str,
         const char* name,
         uint8_t prefix_separator_cnt,
         uint8_t suffix_separator_cnt)),
    API_METHOD(
        nfc_append_filename_string_when_present,
        void,
        (NfcApp * instance, FuriString* string)),
    API_METHOD(nfc_protocol_support_common_submenu_callback, void, (void* context, uint32_t index)),
    API_METHOD(
        nfc_protocol_support_common_widget_callback,
        void,
        (GuiButtonType result, InputType type, void* context)),
    API_METHOD(nfc_protocol_support_common_on_enter_empty, void, (NfcApp * instance)),
    API_METHOD(
        nfc_protocol_support_common_on_event_empty,
        bool,
        (NfcApp * instance, SceneManagerEvent event)),
    API_METHOD(nfc_unlock_helper_setup_from_state, void, (NfcApp * instance)),
    API_METHOD(nfc_unlock_helper_card_detected_handler, void, (NfcApp * instance)),
    API_METHOD(saflok_calculate_checksum, uint8_t, (uint8_t data[BASIC_ACCESS_BYTE_NUM])),
    API_METHOD(saflok_generate_key, void, (const uint8_t* uid, uint8_t* key)),
    API_METHOD(
        saflok_decrypt_card,
        void,
        (uint8_t strCard[BASIC_ACCESS_BYTE_NUM],
         int length,
         uint8_t decryptedCard[BASIC_ACCESS_BYTE_NUM])),
    API_METHOD(
        saflok_encrypt_card,
        void,
        (unsigned char* keyCard, int length, unsigned char* encryptedCard))));
