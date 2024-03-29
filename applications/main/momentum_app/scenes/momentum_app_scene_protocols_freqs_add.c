#include "../momentum_app.h"

enum TextInputResult {
    TextInputResultOk,
    TextInputResultError,
};

static void momentum_app_scene_protocols_freqs_add_text_input_callback(void* context) {
    MomentumApp* app = context;

    char* end;
    uint32_t value = strtol(app->subghz_freq_buffer, &end, 0) * 1000;
    if(*end || !furi_hal_subghz_is_frequency_valid(value)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TextInputResultError);
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
    view_dispatcher_send_custom_event(app->view_dispatcher, TextInputResultOk);
}

void momentum_app_scene_protocols_freqs_add_on_enter(void* context) {
    MomentumApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_set_header_text(text_input, "Ex: 123456 for 123.456 MHz");

    strlcpy(app->subghz_freq_buffer, "", sizeof(app->subghz_freq_buffer));

    text_input_set_result_callback(
        text_input,
        momentum_app_scene_protocols_freqs_add_text_input_callback,
        app,
        app->subghz_freq_buffer,
        sizeof(app->subghz_freq_buffer),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewTextInput);
}

void callback_return(void* context) {
    MomentumApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

bool momentum_app_scene_protocols_freqs_add_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case TextInputResultOk:
            scene_manager_previous_scene(app->scene_manager);
            break;
        case TextInputResultError:
            popup_set_header(app->popup, "Invalid value!", 64, 26, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Frequency was not added...", 64, 40, AlignCenter, AlignCenter);
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
    text_input_reset(app->text_input);
}
