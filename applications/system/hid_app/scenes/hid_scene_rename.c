#include "../hid.h"
#include "../views.h"
#include "hid_icons.h"

static void hid_scene_rename_text_input_callback(void* context) {
    Hid* app = context;

#ifdef HID_TRANSPORT_BLE
    furi_hal_bt_stop_advertising();

    if(app->ble_hid_params == NULL) {
        app->ble_hid_params = malloc(sizeof(BleProfileHidParams));
    }

    if(app->ble_hid_params->device_name_prefix != NULL) {
        free((char*)app->ble_hid_params->device_name_prefix);
    }

    app->ble_hid_params->device_name_prefix = strdup(app->text_input_buffer);
    app->ble_hid_params->skip_device_name = true;

    app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid, app->ble_hid_params);
    furi_check(app->ble_hid_profile);

    furi_hal_bt_start_advertising();
#endif

    // Show popup
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

void hid_scene_rename_popup_callback(void* context) {
    Hid* app = context;

    scene_manager_previous_scene(app->scene_manager);
}

void hid_scene_rename_on_enter(void* context) {
    Hid* app = context;

    // Rename text input view
    text_input_reset(app->text_input);
    text_input_set_result_callback(
        app->text_input,
        hid_scene_rename_text_input_callback,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        false);
    text_input_set_header_text(app->text_input, "Bluetooth Name");

    // Rename success popup view
    popup_set_icon(app->popup, 48, 6, &I_DolphinDone_80x58);
    popup_set_header(app->popup, "Done", 14, 15, AlignLeft, AlignTop);
    popup_set_timeout(app->popup, 1500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, hid_scene_rename_popup_callback);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool hid_scene_rename_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void hid_scene_rename_on_exit(void* context) {
    Hid* app = context;

    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
