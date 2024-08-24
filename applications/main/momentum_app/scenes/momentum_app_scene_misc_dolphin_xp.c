#include "../momentum_app.h"

enum NumberInputResult {
    NumberInputResultOk,
};

static void
    momentum_app_scene_misc_dolphin_xp_number_input_callback(void* context, int32_t number) {
    MomentumApp* app = context;

    app->dolphin_xp = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, NumberInputResultOk);
}

void momentum_app_scene_misc_dolphin_xp_on_enter(void* context) {
    MomentumApp* app = context;
    NumberInput* number_input = app->number_input;

    number_input_set_header_text(number_input, "Enter Dolphin XP value");

    number_input_set_result_callback(
        number_input,
        momentum_app_scene_misc_dolphin_xp_number_input_callback,
        app,
        0,
        0,
        DOLPHIN_MAX_XP);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewNumberInput);
}

bool momentum_app_scene_misc_dolphin_xp_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case NumberInputResultOk:
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_misc_dolphin_xp_on_exit(void* context) {
    MomentumApp* app = context;
    number_input_set_result_callback(app->number_input, NULL, NULL, 0, 0, 0);
    number_input_set_header_text(app->number_input, "");
}
