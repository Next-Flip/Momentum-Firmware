#include "../momentum_app.h"

enum NumberInputResult {
    NumberInputResultOk,
    NumberInputResultError,
};

static void
    momentum_app_scene_protocols_freqs_add_number_input_callback(void* context, int32_t number) {
    MomentumApp* app = context;

    uint32_t value = number * 1000;
    if(!furi_hal_subghz_is_frequency_valid(value)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NumberInputResultError);
        return;
    }
    bool is_hopper =
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneProtocolsFreqsAdd);
    if(is_hopper) {
        FrequencyList_push_back(app->subghz_hopper_freqs, value);
    } else {
        FrequencyList_push_back(app->subghz_static_freqs, value);
    }
    app->save_subghz_freqs = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, NumberInputResultOk);
}

void momentum_app_scene_protocols_freqs_add_on_enter(void* context) {
    MomentumApp* app = context;
    NumberInput* number_input = app->number_input;

    number_input_set_header_text(number_input, "Use kHz values, like 433920");

    number_input_set_result_callback(
        number_input,
        momentum_app_scene_protocols_freqs_add_number_input_callback,
        app,
        0,
        100000,
        999999);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewNumberInput);
}

void callback_return(void* context) {
    MomentumApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewNumberInput);
}

bool momentum_app_scene_protocols_freqs_add_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case NumberInputResultOk:
            scene_manager_previous_scene(app->scene_manager);
            break;
        case NumberInputResultError:
            popup_set_header(app->popup, "Invalid frequency!", 64, 18, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                "Must be 281-361,\n"
                "378-481, 749-962 MHz",
                64,
                40,
                AlignCenter,
                AlignCenter);
            popup_set_callback(app->popup, callback_return);
            popup_set_context(app->popup, app);
            popup_set_timeout(app->popup, 1000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewPopup);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_protocols_freqs_add_on_exit(void* context) {
    MomentumApp* app = context;
    number_input_set_result_callback(app->number_input, NULL, NULL, 0, 0, 0);
    number_input_set_header_text(app->number_input, "");
}
