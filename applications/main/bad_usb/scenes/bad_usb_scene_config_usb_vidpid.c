#include "../bad_usb_app_i.h"

enum ByteInputResult {
    ByteInputResultOk,
};

void bad_usb_scene_config_usb_vidpid_byte_input_callback(void* context) {
    BadUsbApp* bad_usb = context;

    view_dispatcher_send_custom_event(bad_usb->view_dispatcher, ByteInputResultOk);
}

void bad_usb_scene_config_usb_vidpid_on_enter(void* context) {
    BadUsbApp* bad_usb = context;
    ByteInput* byte_input = bad_usb->byte_input;

    bad_usb->usb_vidpid_buf[0] = __REVSH(bad_usb->script_hid_cfg.usb.vid);
    bad_usb->usb_vidpid_buf[1] = __REVSH(bad_usb->script_hid_cfg.usb.pid);
    byte_input_set_header_text(byte_input, "Set USB VID:PID");

    byte_input_set_result_callback(
        byte_input,
        bad_usb_scene_config_usb_vidpid_byte_input_callback,
        NULL,
        bad_usb,
        (void*)bad_usb->usb_vidpid_buf,
        sizeof(bad_usb->usb_vidpid_buf));

    view_dispatcher_switch_to_view(bad_usb->view_dispatcher, BadUsbAppViewByteInput);
}

bool bad_usb_scene_config_usb_vidpid_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* bad_usb = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == ByteInputResultOk) {
            // Apply to current script config
            bad_usb->script_hid_cfg.usb.vid = __REVSH(bad_usb->usb_vidpid_buf[0]);
            bad_usb->script_hid_cfg.usb.pid = __REVSH(bad_usb->usb_vidpid_buf[1]);
            // Set in user config to save in settings file
            bad_usb->user_hid_cfg.usb.vid = bad_usb->script_hid_cfg.usb.vid;
            bad_usb->user_hid_cfg.usb.pid = bad_usb->script_hid_cfg.usb.pid;
        }
        scene_manager_previous_scene(bad_usb->scene_manager);
    }
    return consumed;
}

void bad_usb_scene_config_usb_vidpid_on_exit(void* context) {
    BadUsbApp* bad_usb = context;
    ByteInput* byte_input = bad_usb->byte_input;

    byte_input_set_result_callback(byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(byte_input, "");
}
