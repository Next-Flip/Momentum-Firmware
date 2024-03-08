#include "../momentum_app.h"

enum ByteInputResult {
    ByteInputResultOk,
};

void momentum_app_scene_misc_vgm_color_byte_input_callback(void* context) {
    MomentumApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, ByteInputResultOk);
}

void momentum_app_scene_misc_vgm_color_on_enter(void* context) {
    MomentumApp* app = context;
    ByteInput* byte_input = app->byte_input;

    byte_input_set_header_text(byte_input, "Set VGM Color (RGB565)");

    if(scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneMiscVgmColor)) {
        app->vgm_color = momentum_settings.vgm_color_bg;
    } else {
        app->vgm_color = momentum_settings.vgm_color_fg;
    }
    app->vgm_color.value = __REVSH(app->vgm_color.value);

    byte_input_set_result_callback(
        byte_input,
        momentum_app_scene_misc_vgm_color_byte_input_callback,
        NULL,
        app,
        (void*)&app->vgm_color,
        sizeof(app->vgm_color));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewByteInput);
}

bool momentum_app_scene_misc_vgm_color_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ByteInputResultOk:
            app->vgm_color.value = __REVSH(app->vgm_color.value);
            if(scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneMiscVgmColor)) {
                momentum_settings.vgm_color_bg = app->vgm_color;
            } else {
                momentum_settings.vgm_color_fg = app->vgm_color;
            }
            app->save_settings = true;
            if(momentum_settings.vgm_color_mode == VgmColorModeCustom) {
                expansion_disable(app->expansion);
                expansion_enable(app->expansion);
            }
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_misc_vgm_color_on_exit(void* context) {
    MomentumApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
