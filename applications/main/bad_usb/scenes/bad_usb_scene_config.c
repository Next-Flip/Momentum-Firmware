#include "../bad_usb_app_i.h"

enum ConfigIndex {
    ConfigIndexKeyboardLayout,
    ConfigIndexConnection,
};

enum ConfigIndexBle {
    ConfigIndexBleRemember = ConfigIndexConnection + 1,
    ConfigIndexBlePairing,
    ConfigIndexBleDeviceName,
    ConfigIndexBleMacAddress,
    ConfigIndexBleRandomizeMac,
    ConfigIndexBleUnpair,
};

enum ConfigIndexUsb {
    ConfigIndexUsbManufacturer = ConfigIndexConnection + 1,
    ConfigIndexUsbProductName,
    ConfigIndexUsbVidPid,
    ConfigIndexUsbRandomizeVidPid,
};

void bad_usb_scene_config_connection_callback(VariableItem* item) {
    BadUsbApp* bad_usb = variable_item_get_context(item);
    bad_usb_set_interface(
        bad_usb,
        bad_usb->interface == BadUsbHidInterfaceBle ? BadUsbHidInterfaceUsb :
                                                      BadUsbHidInterfaceBle);
    variable_item_set_current_value_text(
        item, bad_usb->interface == BadUsbHidInterfaceBle ? "BLE" : "USB");
    view_dispatcher_send_custom_event(bad_usb->view_dispatcher, ConfigIndexConnection);
}

void bad_usb_scene_config_ble_remember_callback(VariableItem* item) {
    BadUsbApp* bad_usb = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    // Apply to current script config
    bad_usb->script_hid_cfg.ble.bonding = value;
    // Set in user config to save in settings file
    bad_usb->user_hid_cfg.ble.bonding = value;
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
}

const char* const ble_pairing_names[GapPairingCount] = {
    "YesNo",
    "PIN Type",
    "PIN Y/N",
};
void bad_usb_scene_config_ble_pairing_callback(VariableItem* item) {
    BadUsbApp* bad_usb = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    // Apply to current script config
    bad_usb->script_hid_cfg.ble.pairing = index;
    // Set in user config to save in settings file
    bad_usb->user_hid_cfg.ble.pairing = index;
    variable_item_set_current_value_text(item, ble_pairing_names[index]);
}

void bad_usb_scene_config_select_callback(void* context, uint32_t index) {
    BadUsbApp* bad_usb = context;

    view_dispatcher_send_custom_event(bad_usb->view_dispatcher, index);
}

static void draw_menu(BadUsbApp* bad_usb) {
    VariableItemList* var_item_list = bad_usb->var_item_list;
    VariableItem* item;

    variable_item_list_reset(var_item_list);

    variable_item_list_add(var_item_list, "Keyboard Layout (global)", 0, NULL, NULL);

    item = variable_item_list_add(
        var_item_list, "Connection", 2, bad_usb_scene_config_connection_callback, bad_usb);
    variable_item_set_current_value_index(item, bad_usb->interface == BadUsbHidInterfaceBle);
    variable_item_set_current_value_text(
        item, bad_usb->interface == BadUsbHidInterfaceBle ? "BLE" : "USB");

    if(bad_usb->interface == BadUsbHidInterfaceBle) {
        BleProfileHidParams* ble_hid_cfg = &bad_usb->script_hid_cfg.ble;

        item = variable_item_list_add(
            var_item_list, "BLE Remember", 2, bad_usb_scene_config_ble_remember_callback, bad_usb);
        variable_item_set_current_value_index(item, ble_hid_cfg->bonding);
        variable_item_set_current_value_text(item, ble_hid_cfg->bonding ? "ON" : "OFF");

        item = variable_item_list_add(
            var_item_list,
            "BLE Pairing",
            GapPairingCount,
            bad_usb_scene_config_ble_pairing_callback,
            bad_usb);
        variable_item_set_current_value_index(item, ble_hid_cfg->pairing);
        variable_item_set_current_value_text(item, ble_pairing_names[ble_hid_cfg->pairing]);

        variable_item_list_add(var_item_list, "BLE Device Name", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "BLE MAC Address", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "Randomize BLE MAC", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "Remove BLE Pairing", 0, NULL, NULL);
    } else {
        variable_item_list_add(var_item_list, "USB Manufacturer", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "USB Product Name", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "USB VID and PID", 0, NULL, NULL);

        variable_item_list_add(var_item_list, "Randomize USB VID:PID", 0, NULL, NULL);
    }
}

void bad_usb_scene_config_on_enter(void* context) {
    BadUsbApp* bad_usb = context;
    VariableItemList* var_item_list = bad_usb->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, bad_usb_scene_config_select_callback, bad_usb);
    draw_menu(bad_usb);
    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(bad_usb->scene_manager, BadUsbSceneConfig));

    view_dispatcher_switch_to_view(bad_usb->view_dispatcher, BadUsbAppViewConfig);
}

bool bad_usb_scene_config_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* bad_usb = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(bad_usb->scene_manager, BadUsbSceneConfig, event.event);
        consumed = true;
        switch(event.event) {
        case ConfigIndexKeyboardLayout:
            scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigLayout);
            break;
        case ConfigIndexConnection:
            // Refresh default values for new interface
            const BadUsbHidApi* hid = bad_usb_hid_get_interface(bad_usb->interface);
            hid->adjust_config(&bad_usb->script_hid_cfg);
            // Redraw menu with new interface options
            draw_menu(bad_usb);
            break;
        default:
            break;
        }
        if(bad_usb->interface == BadUsbHidInterfaceBle) {
            switch(event.event) {
            case ConfigIndexBleDeviceName:
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigBleName);
                break;
            case ConfigIndexBleMacAddress:
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigBleMac);
                break;
            case ConfigIndexBleRandomizeMac:
                // Apply to current script config
                furi_hal_random_fill_buf(
                    bad_usb->script_hid_cfg.ble.mac, sizeof(bad_usb->script_hid_cfg.ble.mac));
                // Set in user config to save in settings file
                memcpy(
                    bad_usb->user_hid_cfg.ble.mac,
                    bad_usb->script_hid_cfg.ble.mac,
                    sizeof(bad_usb->user_hid_cfg.ble.mac));
                break;
            case ConfigIndexBleUnpair:
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfirmUnpair);
                break;
            default:
                break;
            }
        } else {
            switch(event.event) {
            case ConfigIndexUsbManufacturer:
                scene_manager_set_scene_state(
                    bad_usb->scene_manager, BadUsbSceneConfigUsbName, true);
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigUsbName);
                break;
            case ConfigIndexUsbProductName:
                scene_manager_set_scene_state(
                    bad_usb->scene_manager, BadUsbSceneConfigUsbName, false);
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigUsbName);
                break;
            case ConfigIndexUsbVidPid:
                scene_manager_next_scene(bad_usb->scene_manager, BadUsbSceneConfigUsbVidPid);
                break;
            case ConfigIndexUsbRandomizeVidPid:
                furi_hal_random_fill_buf(
                    (void*)bad_usb->usb_vidpid_buf, sizeof(bad_usb->usb_vidpid_buf));
                // Apply to current script config
                bad_usb->script_hid_cfg.usb.vid = bad_usb->usb_vidpid_buf[0];
                bad_usb->script_hid_cfg.usb.pid = bad_usb->usb_vidpid_buf[1];
                // Set in user config to save in settings file
                bad_usb->user_hid_cfg.usb.vid = bad_usb->script_hid_cfg.usb.vid;
                bad_usb->user_hid_cfg.usb.pid = bad_usb->script_hid_cfg.usb.pid;
                break;
            default:
                break;
            }
        }
    }

    return consumed;
}

void bad_usb_scene_config_on_exit(void* context) {
    BadUsbApp* bad_usb = context;
    VariableItemList* var_item_list = bad_usb->var_item_list;

    variable_item_list_reset(var_item_list);
}
