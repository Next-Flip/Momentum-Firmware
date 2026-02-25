#include "mf_ultralight.h"
#include "mf_ultralight_render.h"

#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <toolbox/pretty_format.h>

#include "nfc/nfc_app_i.h"

#include "../nfc_protocol_support_common.h"
#include "../nfc_protocol_support_gui_common.h"
#include "../nfc_protocol_support_unlock_helper.h"

enum {
    SubmenuIndexUnlock = SubmenuIndexCommonMax,
    SubmenuIndexUnlockByReader,
    SubmenuIndexUnlockByPassword,
    SubmenuIndexDictAttack,
    SubmenuIndexWriteKeepKey,  // ULC: write data pages, keep target card's existing key
    SubmenuIndexWriteCopyKey,  // ULC: write all pages including key from source card
};

enum {
    NfcSceneMoreInfoStateASCII,
    NfcSceneMoreInfoStateRawData,
};

// ULC write key choice - stored as a static so scene_manager_next_scene can't wipe it.
// Only one write runs at a time so a single static is safe.
static bool s_ulc_write_copy_key = false;

// Tracks whether mf_ultralight_c_dict_context.dict was allocated by OUR write callback.
//
// The stock NfcSceneMfUltralightCDictAttack on_exit calls keys_dict_free() on the dict
// but leaves the dict pointer non-NULL (dangling). If write on_enter blindly calls
// keys_dict_free() on that dangling pointer it double-frees and crashes.
//
// Conversely, if write on_enter skips keys_dict_free() entirely, any dict we opened
// during a previous write's RequestKey phase is leaked (file handle exhaust → crash).
//
// Solution: set this flag whenever OUR RequestKey handler allocates a dict, and
// clear it in write on_enter / RequestMode. We only call keys_dict_free in on_enter
// when this flag is set, guaranteeing the pointer is always ours.
static bool s_ulc_dict_owned = false;

static void nfc_scene_info_on_enter_mf_ultralight(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);

    FuriString* temp_str = furi_string_alloc();
    nfc_append_filename_string_when_present(instance, temp_str);

    furi_string_cat_printf(
        temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));
    furi_string_replace(temp_str, "Mifare", "MIFARE");

    nfc_render_mf_ultralight_info(data, NfcProtocolFormatTypeFull, temp_str);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static void nfc_scene_more_info_on_enter_mf_ultralight(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const MfUltralightData* mfu = nfc_device_get_data(device, NfcProtocolMfUltralight);

    furi_string_reset(instance->text_box_store);
    uint32_t scene_state =
        scene_manager_get_scene_state(instance->scene_manager, NfcSceneMoreInfo);

    if(scene_state == NfcSceneMoreInfoStateASCII) {
        pretty_format_bytes_hex_canonical(
            instance->text_box_store,
            MF_ULTRALIGHT_PAGE_SIZE,
            PRETTY_FORMAT_FONT_MONOSPACE,
            (uint8_t*)mfu->page,
            mfu->pages_read * MF_ULTRALIGHT_PAGE_SIZE);

        widget_add_text_scroll_element(
            instance->widget, 0, 0, 128, 48, furi_string_get_cstr(instance->text_box_store));
        widget_add_button_element(
            instance->widget,
            GuiButtonTypeRight,
            "Raw Data",
            nfc_protocol_support_common_widget_callback,
            instance);

        widget_add_button_element(
            instance->widget,
            GuiButtonTypeLeft,
            "Info",
            nfc_protocol_support_common_widget_callback,
            instance);
    } else if(scene_state == NfcSceneMoreInfoStateRawData) {
        nfc_render_mf_ultralight_dump(mfu, instance->text_box_store);
        widget_add_text_scroll_element(
            instance->widget, 0, 0, 128, 48, furi_string_get_cstr(instance->text_box_store));

        widget_add_button_element(
            instance->widget,
            GuiButtonTypeLeft,
            "ASCII",
            nfc_protocol_support_common_widget_callback,
            instance);
    }
}

static bool nfc_scene_more_info_on_event_mf_ultralight(NfcApp* instance, SceneManagerEvent event) {
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == GuiButtonTypeLeft) ||
       (event.type == SceneManagerEventTypeBack)) {
        scene_manager_set_scene_state(
            instance->scene_manager, NfcSceneMoreInfo, NfcSceneMoreInfoStateASCII);
        scene_manager_previous_scene(instance->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom && event.event == GuiButtonTypeRight) {
        scene_manager_set_scene_state(
            instance->scene_manager, NfcSceneMoreInfo, NfcSceneMoreInfoStateRawData);
        scene_manager_next_scene(instance->scene_manager, NfcSceneMoreInfo);
        consumed = true;
    }
    return consumed;
}

static NfcCommand
    nfc_scene_read_poller_callback_mf_ultralight(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfUltralight);

    NfcApp* instance = context;
    const MfUltralightPollerEvent* mf_ultralight_event = event.event_data;

    if(mf_ultralight_event->type == MfUltralightPollerEventTypeReadSuccess) {
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(instance->poller));

        const MfUltralightData* data =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
        uint32_t event = (data->pages_read == data->pages_total) ? NfcCustomEventPollerSuccess :
                                                                   NfcCustomEventPollerIncomplete;
        view_dispatcher_send_custom_event(instance->view_dispatcher, event);
        return NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthRequest) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventCardDetected);
        nfc_device_set_data(
            instance->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(instance->poller));
        const MfUltralightData* data =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
        if(instance->mf_ul_auth->type == MfUltralightAuthTypeXiaomi) {
            if(mf_ultralight_generate_xiaomi_pass(
                   instance->mf_ul_auth,
                   data->iso14443_3a_data->uid,
                   data->iso14443_3a_data->uid_len)) {
                mf_ultralight_event->data->auth_context.skip_auth = false;
            }
        } else if(instance->mf_ul_auth->type == MfUltralightAuthTypeAmiibo) {
            if(mf_ultralight_generate_amiibo_pass(
                   instance->mf_ul_auth,
                   data->iso14443_3a_data->uid,
                   data->iso14443_3a_data->uid_len)) {
                mf_ultralight_event->data->auth_context.skip_auth = false;
            }
        } else if(
            instance->mf_ul_auth->type == MfUltralightAuthTypeManual ||
            instance->mf_ul_auth->type == MfUltralightAuthTypeReader) {
            mf_ultralight_event->data->auth_context.skip_auth = false;
        } else {
            mf_ultralight_event->data->auth_context.skip_auth = true;
        }
        if(!mf_ultralight_event->data->auth_context.skip_auth) {
            mf_ultralight_event->data->auth_context.password = instance->mf_ul_auth->password;

            if(data->type == MfUltralightTypeMfulC) {
                // Only set tdes_key for Manual/Reader auth types, not for dictionary attacks
                if(instance->mf_ul_auth->type == MfUltralightAuthTypeManual ||
                   instance->mf_ul_auth->type == MfUltralightAuthTypeReader) {
                    mf_ultralight_event->data->key_request_data.key =
                        instance->mf_ul_auth->tdes_key;
                    mf_ultralight_event->data->key_request_data.key_provided = true;
                } else {
                    mf_ultralight_event->data->key_request_data.key_provided = false;
                }
            }
        }
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthSuccess) {
        instance->mf_ul_auth->pack = mf_ultralight_event->data->auth_context.pack;
    }

    return NfcCommandContinue;
}

static void nfc_scene_read_on_enter_mf_ultralight(NfcApp* instance) {
    nfc_unlock_helper_setup_from_state(instance);
    nfc_poller_start(instance->poller, nfc_scene_read_poller_callback_mf_ultralight, instance);
}

bool nfc_scene_read_on_event_mf_ultralight(NfcApp* instance, SceneManagerEvent event) {
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventPollerSuccess) {
            notification_message(instance->notifications, &sequence_success);
            scene_manager_next_scene(instance->scene_manager, NfcSceneReadSuccess);
            dolphin_deed(DolphinDeedNfcReadSuccess);
            return true;
        } else if(event.event == NfcCustomEventPollerIncomplete) {
            const MfUltralightData* data =
                nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
            if(data->type == MfUltralightTypeMfulC &&
               instance->mf_ul_auth->type == MfUltralightAuthTypeNone) {
                // Start dict attack for MFUL C cards only if no specific auth was attempted
                scene_manager_next_scene(instance->scene_manager, NfcSceneMfUltralightCDictAttack);
            } else {
                if(data->pages_read == data->pages_total) {
                    notification_message(instance->notifications, &sequence_success);
                } else {
                    notification_message(instance->notifications, &sequence_semi_success);
                }
                scene_manager_next_scene(instance->scene_manager, NfcSceneReadSuccess);
                dolphin_deed(DolphinDeedNfcReadSuccess);
            }
            return true;
        }
    }
    return false;
}

static void nfc_scene_read_and_saved_menu_on_enter_mf_ultralight(NfcApp* instance) {
    Submenu* submenu = instance->submenu;

    const MfUltralightData* data =
        nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
    bool is_locked = !mf_ultralight_is_all_data_read(data);
    bool is_mfulc = (data->type == MfUltralightTypeMfulC);

    if(is_locked ||
       (data->type != MfUltralightTypeNTAG213 && data->type != MfUltralightTypeNTAG215 &&
        data->type != MfUltralightTypeNTAG216 && data->type != MfUltralightTypeUL11 &&
        data->type != MfUltralightTypeUL21 && data->type != MfUltralightTypeOrigin &&
        data->type != MfUltralightTypeMfulC)) {
        submenu_remove_item(submenu, SubmenuIndexCommonWrite);
    } else if(is_mfulc) {
        // Replace the generic Write item with two ULC-specific options so the user
        // can choose whether to keep or overwrite the target card's 3DES key.
        // This avoids any mid-write dialog/view-switching complexity entirely.
        submenu_remove_item(submenu, SubmenuIndexCommonWrite);
        submenu_add_item(
            submenu,
            "Write (Keep Key)",
            SubmenuIndexWriteKeepKey,
            nfc_protocol_support_common_submenu_callback,
            instance);
        submenu_add_item(
            submenu,
            "Write (Copy Key)",
            SubmenuIndexWriteCopyKey,
            nfc_protocol_support_common_submenu_callback,
            instance);
    }

    if(is_locked) {
        submenu_add_item(
            submenu,
            "Unlock",
            SubmenuIndexUnlock,
            nfc_protocol_support_common_submenu_callback,
            instance);
        if(data->type == MfUltralightTypeMfulC) {
            submenu_add_item(
                submenu,
                "Unlock with Dictionary",
                SubmenuIndexDictAttack,
                nfc_protocol_support_common_submenu_callback,
                instance);
        }
    }
}

static void nfc_scene_read_success_on_enter_mf_ultralight(NfcApp* instance) {
    const NfcDevice* device = instance->nfc_device;
    const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);

    FuriString* temp_str = furi_string_alloc();

    bool unlocked =
        scene_manager_has_previous_scene(instance->scene_manager, NfcSceneMfUltralightUnlockWarn);
    if(unlocked) {
        nfc_render_mf_ultralight_pwd_pack(data, temp_str);
    } else {
        furi_string_cat_printf(
            temp_str, "\e#%s\n", nfc_device_get_name(device, NfcDeviceNameTypeFull));

        furi_string_replace(temp_str, "Mifare", "MIFARE");

        nfc_render_mf_ultralight_info(data, NfcProtocolFormatTypeShort, temp_str);
    }

    mf_ultralight_auth_reset(instance->mf_ul_auth);

    widget_add_text_scroll_element(
        instance->widget, 0, 0, 128, 52, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static void nfc_scene_emulate_on_enter_mf_ultralight(NfcApp* instance) {
    const MfUltralightData* data =
        nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
    instance->listener = nfc_listener_alloc(instance->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(instance->listener, NULL, NULL);
}

static bool nfc_scene_read_and_saved_menu_on_event_mf_ultralight(
    NfcApp* instance,
    SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexUnlock) {
            const MfUltralightData* data =
                nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);

            uint32_t next_scene = (data->type == MfUltralightTypeMfulC) ?
                                      NfcSceneDesAuthKeyInput :
                                      NfcSceneMfUltralightUnlockMenu;
            scene_manager_next_scene(instance->scene_manager, next_scene);
            consumed = true;
        } else if(event.event == SubmenuIndexDictAttack) {
            if(!scene_manager_search_and_switch_to_previous_scene(
                   instance->scene_manager, NfcSceneMfUltralightCDictAttack)) {
                scene_manager_next_scene(instance->scene_manager, NfcSceneMfUltralightCDictAttack);
            }
            consumed = true;
        } else if(event.event == SubmenuIndexWriteKeepKey) {
            s_ulc_write_copy_key = false;
            FURI_LOG_D("MfULC", "Write menu: KEEP KEY selected");
            scene_manager_next_scene(instance->scene_manager, NfcSceneWrite);
            consumed = true;
        } else if(event.event == SubmenuIndexWriteCopyKey) {
            s_ulc_write_copy_key = true;
            FURI_LOG_D("MfULC", "Write menu: COPY KEY selected");
            scene_manager_next_scene(instance->scene_manager, NfcSceneWrite);
            consumed = true;
        }
    }
    return consumed;
}

static NfcCommand
    nfc_scene_write_poller_callback_mf_ultralight(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfUltralight);

    NfcApp* instance = context;
    MfUltralightPollerEvent* mf_ultralight_event = event.event_data;
    NfcCommand command = NfcCommandContinue;

    if(mf_ultralight_event->type == MfUltralightPollerEventTypeRequestMode) {
        mf_ultralight_event->data->poller_mode = MfUltralightPollerModeWrite;
        furi_string_reset(instance->text_box_store);
        // Free any dict handle left open by the read phase before resetting context.
        // Without this, every write-scene entry leaks a file handle (one per card read).
        if(instance->mf_ultralight_c_dict_context.dict) {
            keys_dict_free(instance->mf_ultralight_c_dict_context.dict);
        }
        instance->mf_ultralight_c_dict_context.dict = NULL;
        instance->mf_ultralight_c_dict_context.dict_keys_current = 0;
        s_ulc_dict_owned = false;
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventCardDetected);
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthRequest) {
        // Skip auth during the read phase of write - we'll authenticate
        // against the target card in RequestWriteData using source key or dict attack
        mf_ultralight_event->data->auth_context.skip_auth = true;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeRequestKey) {
        // Dict attack key provider - checks cache first, then user dict, then system dict
        if(!instance->mf_ultralight_c_dict_context.dict &&
           instance->mf_ultralight_c_dict_context.dict_keys_current != 100) {
            // First call - try cached key before opening dictionary
            if(instance->mf_ultralight_c_dict_context.dict_keys_current == 0) {
                // Use target_uid from event data - the poller sets this from instance->data
                // (the actual target card), NOT from nfc_device which holds source card data.
                uint8_t uid_len = mf_ultralight_event->data->key_request_data.target_uid_len;
                const uint8_t* uid = mf_ultralight_event->data->key_request_data.target_uid;

                if(uid_len > 0) {
                    char uid_hex[32] = {0};
                    for(uint8_t i = 0; i < uid_len && i < 10; i++) {
                        snprintf(uid_hex + i * 2, sizeof(uid_hex) - i * 2, "%02X", uid[i]);
                    }

                    Storage* storage = furi_record_open(RECORD_STORAGE);
                    File* file = storage_file_alloc(storage);
                    if(storage_file_open(
                           file,
                           "/ext/nfc/assets/mf_ultralight_c_key_cache.nfc",
                           FSAM_READ,
                           FSOM_OPEN_EXISTING)) {
                        FuriString* content = furi_string_alloc();
                        char buf[128];
                        size_t read;
                        while((read = storage_file_read(file, buf, sizeof(buf) - 1)) > 0) {
                            buf[read] = '\0';
                            furi_string_cat_str(content, buf);
                        }
                        storage_file_close(file);

                        // Parse "UID: <uid>\nKey: <key>" format.
                        // Find the UID line, then grab the Key: on the very next line.
                        FuriString* uid_tag = furi_string_alloc_printf("UID: %s", uid_hex);
                        size_t uid_pos = furi_string_search(content, uid_tag, 0);
                        if(uid_pos != FURI_STRING_FAILURE) {
                            size_t key_tag_pos =
                                furi_string_search_str(content, "Key: ", uid_pos);
                            if(key_tag_pos != FURI_STRING_FAILURE) {
                                size_t key_start = key_tag_pos + 5; // skip "Key: "
                                const char* cstr = furi_string_get_cstr(content);
                                MfUltralightC3DesAuthKey cached_key = {};
                                bool parse_ok = true;
                                for(int i = 0; i < 16 && parse_ok; i++) {
                                    char hi = cstr[key_start + i * 2];
                                    char lo = cstr[key_start + i * 2 + 1];
                                    if(!hi || !lo || hi == '\n' || lo == '\n') {
                                        parse_ok = false;
                                    } else {
                                        uint8_t val = 0;
                                        if(hi >= '0' && hi <= '9')
                                            val = (hi - '0') << 4;
                                        else if(hi >= 'A' && hi <= 'F')
                                            val = (hi - 'A' + 10) << 4;
                                        else if(hi >= 'a' && hi <= 'f')
                                            val = (hi - 'a' + 10) << 4;
                                        else
                                            parse_ok = false;
                                        if(lo >= '0' && lo <= '9')
                                            val |= (lo - '0');
                                        else if(lo >= 'A' && lo <= 'F')
                                            val |= (lo - 'A' + 10);
                                        else if(lo >= 'a' && lo <= 'f')
                                            val |= (lo - 'a' + 10);
                                        else
                                            parse_ok = false;
                                        cached_key.data[i] = val;
                                    }
                                }
                                if(parse_ok) {
                                    FURI_LOG_D("MfULC", "Found cached key for UID %s", uid_hex);
                                    mf_ultralight_event->data->key_request_data.key = cached_key;
                                    mf_ultralight_event->data->key_request_data.key_provided = true;
                                    furi_string_free(uid_tag);
                                    furi_string_free(content);
                                    storage_file_free(file);
                                    furi_record_close(RECORD_STORAGE);
                                    instance->mf_ultralight_c_dict_context.dict_keys_current = 99;
                                    return NfcCommandContinue;
                                }
                            }
                        }
                        furi_string_free(uid_tag);
                        furi_string_free(content);
                    } else {
                        storage_file_close(file);
                    }
                    storage_file_free(file);
                    furi_record_close(RECORD_STORAGE);
                }
            }

            // Open user dict first if it exists, otherwise system dict
            if(keys_dict_check_presence(NFC_APP_MF_ULTRALIGHT_C_DICT_USER_PATH)) {
                instance->mf_ultralight_c_dict_context.dict = keys_dict_alloc(
                    NFC_APP_MF_ULTRALIGHT_C_DICT_USER_PATH,
                    KeysDictModeOpenExisting,
                    sizeof(MfUltralightC3DesAuthKey));
                instance->mf_ultralight_c_dict_context.dict_keys_current = 1;
                s_ulc_dict_owned = true;
            }
            if(!instance->mf_ultralight_c_dict_context.dict) {
                instance->mf_ultralight_c_dict_context.dict = keys_dict_alloc(
                    NFC_APP_MF_ULTRALIGHT_C_DICT_SYSTEM_PATH,
                    KeysDictModeOpenExisting,
                    sizeof(MfUltralightC3DesAuthKey));
                instance->mf_ultralight_c_dict_context.dict_keys_current = 2;
                s_ulc_dict_owned = true;
            }
        }
        MfUltralightC3DesAuthKey key = {};
        bool got_key = false;
        if(instance->mf_ultralight_c_dict_context.dict) {
            got_key = keys_dict_get_next_key(
                instance->mf_ultralight_c_dict_context.dict,
                key.data,
                sizeof(MfUltralightC3DesAuthKey));
        }
        if(!got_key && instance->mf_ultralight_c_dict_context.dict_keys_current < 2) {
            // Exhausted user dict, switch to system dict
            if(instance->mf_ultralight_c_dict_context.dict) {
                keys_dict_free(instance->mf_ultralight_c_dict_context.dict);
            }
            instance->mf_ultralight_c_dict_context.dict = keys_dict_alloc(
                NFC_APP_MF_ULTRALIGHT_C_DICT_SYSTEM_PATH,
                KeysDictModeOpenExisting,
                sizeof(MfUltralightC3DesAuthKey));
            instance->mf_ultralight_c_dict_context.dict_keys_current = 2;
            s_ulc_dict_owned = true;
            if(instance->mf_ultralight_c_dict_context.dict) {
                got_key = keys_dict_get_next_key(
                    instance->mf_ultralight_c_dict_context.dict,
                    key.data,
                    sizeof(MfUltralightC3DesAuthKey));
            }
        }
        if(got_key) {
            mf_ultralight_event->data->key_request_data.key = key;
            mf_ultralight_event->data->key_request_data.key_provided = true;
            FURI_LOG_D(
                "MfULC",
                "Trying dict key: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                key.data[0],  key.data[1],  key.data[2],  key.data[3],
                key.data[4],  key.data[5],  key.data[6],  key.data[7],
                key.data[8],  key.data[9],  key.data[10], key.data[11],
                key.data[12], key.data[13], key.data[14], key.data[15]);
        } else {
            mf_ultralight_event->data->key_request_data.key_provided = false;
            FURI_LOG_D("MfULC", "Dict exhausted - no more keys");
            if(instance->mf_ultralight_c_dict_context.dict) {
                keys_dict_free(instance->mf_ultralight_c_dict_context.dict);
                instance->mf_ultralight_c_dict_context.dict = NULL;
            }
            // Sentinel: prevent the if(!dict) block from re-opening dicts on the next call.
            // Without this, after exhaustion dict==NULL resets the state machine into
            // an infinite loop of re-opening and re-reading both dictionaries.
            instance->mf_ultralight_c_dict_context.dict_keys_current = 100;
            s_ulc_dict_owned = false;
        }
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeRequestWriteData) {
        mf_ultralight_event->data->write_data =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
        // Reset dict context for write-phase key search (fresh start: cache check, then dicts)
        if(instance->mf_ultralight_c_dict_context.dict) {
            keys_dict_free(instance->mf_ultralight_c_dict_context.dict);
            instance->mf_ultralight_c_dict_context.dict = NULL;
        }
        // Reset to 0 so cache lookup runs first (dict_keys_current==0 triggers cache check)
        instance->mf_ultralight_c_dict_context.dict_keys_current = 0;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeWriteKeyRequest) {
        // ULC write: key_request_data has the successful auth key and target_uid.
        // Cache under the TARGET card's UID (from event data, set by poller from instance->data).
        // write_key_skip is in the same union - read key data FIRST, then write the skip flag.

        // Snapshot key at top level so it's in scope for the log after the if block closes.
        MfUltralightC3DesAuthKey found_key = {};
        bool found_key_valid = false;

        if(mf_ultralight_event->data->key_request_data.key_provided) {
            found_key = mf_ultralight_event->data->key_request_data.key;
            found_key_valid = true;
            uint8_t uid_len = mf_ultralight_event->data->key_request_data.target_uid_len;
            uint8_t uid_buf[10];
            memcpy(uid_buf, mf_ultralight_event->data->key_request_data.target_uid, uid_len);

            if(uid_len > 0) {
                char uid_hex[32] = {0};
                for(uint8_t i = 0; i < uid_len && i < 10; i++) {
                    snprintf(uid_hex + i * 2, sizeof(uid_hex) - i * 2, "%02X", uid_buf[i]);
                }

                char key_hex[48] = {0};
                for(uint8_t i = 0; i < 16; i++) {
                    snprintf(
                        key_hex + i * 2,
                        sizeof(key_hex) - i * 2,
                        "%02X",
                        found_key.data[i]);
                }

                Storage* storage = furi_record_open(RECORD_STORAGE);
                storage_common_mkdir(storage, "/ext/nfc/assets");
                File* file = storage_file_alloc(storage);

                FuriString* cache_content = furi_string_alloc();
                FuriString* new_content = furi_string_alloc();

                // Write header
                furi_string_cat_str(
                    new_content, "Filetype: Flipper NFC ULC Key Cache\nVersion: 1\n");

                if(storage_file_open(
                       file,
                       "/ext/nfc/assets/mf_ultralight_c_key_cache.nfc",
                       FSAM_READ,
                       FSOM_OPEN_EXISTING)) {
                    char buf[128];
                    size_t read;
                    while((read = storage_file_read(file, buf, sizeof(buf) - 1)) > 0) {
                        buf[read] = '\0';
                        furi_string_cat_str(cache_content, buf);
                    }
                    storage_file_close(file);

                    // Re-emit all existing UID/Key pairs, skipping the one we're updating
                    FuriString* line = furi_string_alloc();
                    FuriString* uid_tag = furi_string_alloc_printf("UID: %s", uid_hex);
                    bool skip_next_key = false;
                    size_t pos = 0;
                    while(pos < furi_string_size(cache_content)) {
                        size_t nl = furi_string_search_char(cache_content, '\n', pos);
                        if(nl == FURI_STRING_FAILURE) nl = furi_string_size(cache_content);
                        furi_string_set_n(line, cache_content, pos, nl - pos);

                        if(furi_string_start_with_str(line, "UID: ")) {
                            skip_next_key = furi_string_equal(line, uid_tag);
                            if(!skip_next_key) {
                                furi_string_cat_str(new_content, "\n");
                                furi_string_cat(new_content, line);
                                furi_string_cat_str(new_content, "\n");
                            }
                        } else if(furi_string_start_with_str(line, "Key: ")) {
                            if(!skip_next_key) {
                                furi_string_cat(new_content, line);
                                furi_string_cat_str(new_content, "\n");
                            }
                            skip_next_key = false;
                        }
                        // Skip header lines and blanks (re-emitted above)
                        pos = nl + 1;
                    }
                    furi_string_free(uid_tag);
                    furi_string_free(line);
                } else {
                    storage_file_close(file);
                }

                // Append new entry
                furi_string_cat_printf(
                    new_content, "\nUID: %s\nKey: %s\n", uid_hex, key_hex);

                if(storage_file_open(
                       file,
                       "/ext/nfc/assets/mf_ultralight_c_key_cache.nfc",
                       FSAM_WRITE,
                       FSOM_CREATE_ALWAYS)) {
                    storage_file_write(
                        file,
                        furi_string_get_cstr(new_content),
                        furi_string_size(new_content));
                    storage_file_close(file);
                    FURI_LOG_D("MfULC", "Cached key for target UID %s", uid_hex);
                }

                furi_string_free(cache_content);
                furi_string_free(new_content);
                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);
            }
        }

        // Apply the user's key choice - read from static, not scene state (scene manager
        // resets state to 0 on scene entry, wiping any value set before next_scene).
        // Set write_key_skip LAST - it shares the union with key_request_data above.
        bool keep_key = !s_ulc_write_copy_key;
        mf_ultralight_event->data->write_key_skip = keep_key;

        // Log the found key and what's in source pages 44-47 so there's no mystery
        if(found_key_valid) {
            FURI_LOG_D(
                "MfULC",
                "WriteKeyRequest: target card auth key = %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                found_key.data[0],  found_key.data[1],  found_key.data[2],  found_key.data[3],
                found_key.data[4],  found_key.data[5],  found_key.data[6],  found_key.data[7],
                found_key.data[8],  found_key.data[9],  found_key.data[10], found_key.data[11],
                found_key.data[12], found_key.data[13], found_key.data[14], found_key.data[15]);
        }

        // Log what the source file has in pages 44-47
        const MfUltralightData* src =
            nfc_device_get_data(instance->nfc_device, NfcProtocolMfUltralight);
        if(src) {
            uint8_t src_key[16];
            for(int i = 0; i < 4; i++) memcpy(&src_key[i * 4], src->page[44 + i].data, 4);
            bool all_zero = true;
            for(int i = 0; i < 16; i++) if(src_key[i]) { all_zero = false; break; }
            FURI_LOG_D(
                "MfULC",
                "WriteKeyRequest: source file pages 44-47 = %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%s",
                src_key[0],  src_key[1],  src_key[2],  src_key[3],
                src_key[4],  src_key[5],  src_key[6],  src_key[7],
                src_key[8],  src_key[9],  src_key[10], src_key[11],
                src_key[12], src_key[13], src_key[14], src_key[15],
                all_zero ? " [ALL ZEROS - Copy Key will be refused by poller]" : "");
        }
        FURI_LOG_D(
            "MfULC",
            "WriteKeyRequest: decision = %s (s_ulc_write_copy_key=%d)",
            keep_key ? "KEEP target key (pages 44-47 NOT written)" :
                       "OVERWRITE with source key (pages 44-47 WILL be written)",
            (int)s_ulc_write_copy_key);
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeCardMismatch) {
        furi_string_set(instance->text_box_store, "Card of the same\ntype should be\n presented");
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventWrongCard);
        command = NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeCardLocked) {
        furi_string_set(
            instance->text_box_store, "Card protected by\npassword, AUTH0\nor lock bits");
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventPollerFailure);
        command = NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeWriteFail) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventPollerFailure);
        command = NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeWriteSuccess) {
        furi_string_reset(instance->text_box_store);
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventPollerSuccess);
        command = NfcCommandStop;
    }

    return command;
}

static void nfc_scene_write_on_enter_mf_ultralight(NfcApp* instance) {
    // Key choice (Keep/Copy) was set in scene state by the saved menu before
    // scene_manager_next_scene(NfcSceneWrite) was called.
    //
    // The framework stops and frees instance->poller when exiting the read/menu
    // scenes, so we MUST allocate a fresh poller here. Do NOT skip this alloc --
    // calling nfc_poller_start on a NULL pointer will furi_assert and hard-reset.
    //
    // Zero the dict context, freeing only if WE opened the dict.
    //
    // After a DictAttack read, the stock scene's on_exit calls keys_dict_free() but
    // leaves the pointer non-NULL (dangling). Freeing it here would double-free.
    // After a previous write that needed a dict attack, the dict IS still open and
    // MUST be freed here or we leak a file handle (leading to crash on next dict open).
    // s_ulc_dict_owned tells us which situation we're in.
    if(s_ulc_dict_owned && instance->mf_ultralight_c_dict_context.dict) {
        keys_dict_free(instance->mf_ultralight_c_dict_context.dict);
    }
    instance->mf_ultralight_c_dict_context.dict = NULL;
    instance->mf_ultralight_c_dict_context.dict_keys_current = 0;
    s_ulc_dict_owned = false;
    furi_string_set(instance->text_box_store, "\nApply the\ntarget\ncard now");
    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(instance->poller, nfc_scene_write_poller_callback_mf_ultralight, instance);
}

static bool nfc_scene_write_on_event_mf_ultralight(NfcApp* instance, SceneManagerEvent event) {
    UNUSED(instance);
    UNUSED(event);
    return false;
}

const NfcProtocolSupportBase nfc_protocol_support_mf_ultralight = {
    .features = NfcProtocolFeatureEmulateFull | NfcProtocolFeatureMoreInfo |
                NfcProtocolFeatureWrite,

    .scene_info =
        {
            .on_enter = nfc_scene_info_on_enter_mf_ultralight,
            .on_event = nfc_protocol_support_common_on_event_empty,
        },
    .scene_more_info =
        {
            .on_enter = nfc_scene_more_info_on_enter_mf_ultralight,
            .on_event = nfc_scene_more_info_on_event_mf_ultralight,
        },
    .scene_read =
        {
            .on_enter = nfc_scene_read_on_enter_mf_ultralight,
            .on_event = nfc_scene_read_on_event_mf_ultralight,
        },
    .scene_read_menu =
        {
            .on_enter = nfc_scene_read_and_saved_menu_on_enter_mf_ultralight,
            .on_event = nfc_scene_read_and_saved_menu_on_event_mf_ultralight,
        },
    .scene_read_success =
        {
            .on_enter = nfc_scene_read_success_on_enter_mf_ultralight,
            .on_event = nfc_protocol_support_common_on_event_empty,
        },
    .scene_saved_menu =
        {
            .on_enter = nfc_scene_read_and_saved_menu_on_enter_mf_ultralight,
            .on_event = nfc_scene_read_and_saved_menu_on_event_mf_ultralight,
        },
    .scene_save_name =
        {
            .on_enter = nfc_protocol_support_common_on_enter_empty,
            .on_event = nfc_protocol_support_common_on_event_empty,
        },
    .scene_emulate =
        {
            .on_enter = nfc_scene_emulate_on_enter_mf_ultralight,
            .on_event = nfc_protocol_support_common_on_event_empty,
        },
    .scene_write =
        {
            .on_enter = nfc_scene_write_on_enter_mf_ultralight,
            .on_event = nfc_scene_write_on_event_mf_ultralight,
        },
};

NFC_PROTOCOL_SUPPORT_PLUGIN(mf_ultralight, NfcProtocolMfUltralight);
