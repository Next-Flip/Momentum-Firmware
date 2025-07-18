#include "../hid.h"
#include "../views.h"
#include "hid_icons.h"

#include <ble/ble.h>

// AN5289: 4.7, in order to use flash controller interval must be at least 25ms + advertisement, which is 30 ms
// Since we don't use flash controller anymore interval can be lowered to 7.5ms
#define CONNECTION_INTERVAL_MIN (0x0006)
// Up to 45 ms
#define CONNECTION_INTERVAL_MAX (0x24)

static GapConfig template_config = {
    .adv_service =
        {
            .UUID_Type = UUID_TYPE_16,
            .Service_UUID_16 = HUMAN_INTERFACE_DEVICE_SERVICE_UUID,
        },
    .appearance_char = GAP_APPEARANCE_KEYBOARD,
    .bonding_mode = true,
    .pairing_method = GapPairingPinCodeVerifyYesNo,
    .conn_param =
        {
            .conn_int_min = CONNECTION_INTERVAL_MIN,
            .conn_int_max = CONNECTION_INTERVAL_MAX,
            .slave_latency = 0,
            .supervisor_timeout = 0,
        },
};

// Uses the profile_params as a char* for the advertised device name
static void custom_get_gap_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    furi_check(config);
    memcpy(config, &template_config, sizeof(GapConfig));
    // Set mac address
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));

    // Change MAC address for HID profile
    config->mac_address[2]++;

    // Set advertise name
    snprintf(
        config->adv_name,
        sizeof(config->adv_name),
        "%c%s",
        furi_hal_version_get_ble_local_device_name_ptr()[0],
        (char*)profile_params);
}

static void hid_scene_rename_text_input_callback(void* context) {
    Hid* app = context;

#ifdef HID_TRANSPORT_BLE
    furi_hal_bt_stop_advertising();

    // Reuse existing start and stop methods from default profile,
    //  but use modified get_gap_config (to set custom name)
    FuriHalBleProfileTemplate profile = {
        .start = ble_profile_hid->start,
        .stop = ble_profile_hid->stop,
        .get_gap_config = custom_get_gap_config,
    };

    app->ble_hid_profile = bt_profile_start(app->bt, &profile, app->text_input_buffer);
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
