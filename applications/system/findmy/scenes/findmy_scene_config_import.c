#include "../findmy_i.h"

enum VarItemListIndex {
    VarItemListIndexOpenHaystack,
};

static const char* parse_open_haystack(FindMy* app, const char* path) {
    const char* error = NULL;

    Stream* stream = file_stream_alloc(app->storage);
    FuriString* line = furi_string_alloc();
    do {
        error = "Can't open file";
        if(!file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        error = "Wrong file format";
        while(stream_read_line(stream, line)) {
            if(furi_string_start_with(line, "Public key: ") ||
               furi_string_start_with(line, "Advertisement key: ")) {
                error = NULL;
                break;
            }
        }
        if(error) break;

        furi_string_right(line, furi_string_search_char(line, ':') + 2);
        furi_string_trim(line);

        error = "Base64 failed";
        size_t decoded_len;
        uint8_t* public_key = base64_decode(
            (uint8_t*)furi_string_get_cstr(line), furi_string_size(line), &decoded_len);
        if(decoded_len != 28) {
            free(public_key);
            break;
        }

        memcpy(app->state.mac, public_key, sizeof(app->state.mac));
        app->state.mac[0] |= 0b11000000;
        furi_hal_bt_reverse_mac_addr(app->state.mac);

        uint8_t advertisement_template[EXTRA_BEACON_MAX_DATA_SIZE] = {
            0x1e, // length (30)
            0xff, // manufacturer specific data
            0x4c, 0x00, // company ID (Apple)
            0x12, 0x19, // offline finding type and length
            0x00, //state
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, // first two bits of key[0]
            0x00, // hint
        };
        memcpy(app->state.data, advertisement_template, sizeof(app->state.data));
        memcpy(&app->state.data[7], &public_key[6], decoded_len - 6);
        app->state.data[29] = public_key[0] >> 6;
        findmy_state_sync_config(&app->state);
        findmy_state_save(&app->state);

        free(public_key);
        error = NULL;

    } while(false);
    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);

    return error;
}

void findmy_scene_config_import_callback(void* context, uint32_t index) {
    furi_assert(context);
    FindMy* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void findmy_scene_config_import_on_enter(void* context) {
    FindMy* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "OpenHaystack .keys", 0, NULL, NULL);

    // This scene acts more like a submenu than a var item list tbh
    UNUSED(item);

    variable_item_list_set_enter_callback(var_item_list, findmy_scene_config_import_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, FindMySceneConfigImport));

    view_dispatcher_switch_to_view(app->view_dispatcher, FindMyViewVarItemList);
}

bool findmy_scene_config_import_on_event(void* context, SceneManagerEvent event) {
    FindMy* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, FindMySceneConfigImport, event.event);
        consumed = true;

        const char* extension = NULL;
        switch(event.event) {
        case VarItemListIndexOpenHaystack:
            extension = ".keys";
            break;
        default:
            break;
        }
        if(!extension) {
            return consumed;
        }

        const DialogsFileBrowserOptions browser_options = {
            .extension = extension,
            .icon = &I_text_10px,
            .base_path = FINDMY_STATE_DIR,
        };
        storage_simply_mkdir(app->storage, browser_options.base_path);
        FuriString* path = furi_string_alloc_set_str(browser_options.base_path);
        if(dialog_file_browser_show(app->dialogs, path, path, &browser_options)) {
            // The parse functions return the error text, or NULL for success
            // Used in result to show success or error message
            const char* error = NULL;
            switch(event.event) {
            case VarItemListIndexOpenHaystack:
                error = parse_open_haystack(app, furi_string_get_cstr(path));
                break;
            }
            scene_manager_set_scene_state(
                app->scene_manager, FindMySceneConfigImportResult, (uint32_t)error);
            scene_manager_next_scene(app->scene_manager, FindMySceneConfigImportResult);
        }
        furi_string_free(path);
    }

    return consumed;
}

void findmy_scene_config_import_on_exit(void* context) {
    FindMy* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_reset(var_item_list);
}