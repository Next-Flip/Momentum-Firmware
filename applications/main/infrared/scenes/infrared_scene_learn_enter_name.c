#include "../infrared_app_i.h"
#include <dolphin/dolphin.h>

static const char* const easy_mode_button_names[] = {
    "Power", "Vol_Up", "Vol_Down", "Ch_Up", "Ch_Down", "Mute",
    "Menu", "Input", "Back", "Ok", "Up", "Down", "Left", "Right",
    "Play", "Pause", "Stop", "Prev", "Next", "Rew", "FF"
};

void infrared_scene_learn_enter_name_on_enter(void* context) {
    InfraredApp* infrared = context;
    TextInput* text_input = infrared->text_input;
    InfraredSignal* signal = infrared->current_signal;

    if(infrared->app_state.is_easy_mode) {
        // In easy mode, use predefined names based on button count
        size_t button_count = infrared_remote_get_signal_count(infrared->remote);
        if(button_count < COUNT_OF(easy_mode_button_names)) {
            infrared_text_store_set(infrared, 0, "%s", easy_mode_button_names[button_count]);
        } else {
            infrared_text_store_set(infrared, 0, "Button_%d", button_count + 1);
        }
    } else if(infrared_signal_is_raw(signal)) {
        const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
        infrared_text_store_set(infrared, 0, "RAW_%zu", raw->timings_size);
    } else {
        const InfraredMessage* message = infrared_signal_get_message(signal);
        infrared_text_store_set(
            infrared,
            0,
            "%.4s_%0*lX",
            infrared_get_protocol_name(message->protocol),
            ROUND_UP_TO(infrared_get_protocol_command_length(message->protocol), 4),
            message->command);
    }

    text_input_set_header_text(text_input, "Name the button");
    text_input_set_result_callback(
        text_input,
        infrared_text_input_callback,
        context,
        infrared->text_store[0],
        INFRARED_MAX_BUTTON_NAME_LENGTH,
        !infrared->app_state.is_easy_mode); // Only allow editing in normal mode

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewTextInput);
}

bool infrared_scene_learn_enter_name_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    InfraredSignal* signal = infrared->current_signal;
    SceneManager* scene_manager = infrared->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InfraredCustomEventTypeTextEditDone) {
            const char* signal_name = infrared->text_store[0];
            const InfraredErrorCode error =
                infrared->app_state.is_learning_new_remote ?
                    infrared_add_remote_with_button(infrared, signal_name, signal) :
                    infrared_remote_append_signal(infrared->remote, signal, signal_name);

            if(!INFRARED_ERROR_PRESENT(error)) {
                scene_manager_next_scene(scene_manager, InfraredSceneLearnDone);
                dolphin_deed(DolphinDeedIrSave);
            } else {
                infrared_show_error_message(
                    infrared,
                    "Failed to\n%s",
                    infrared->app_state.is_learning_new_remote ? "create file" : "add signal");
                const uint32_t possible_scenes[] = {InfraredSceneRemoteList, InfraredSceneStart};
                scene_manager_search_and_switch_to_previous_scene_one_of(
                    scene_manager, possible_scenes, COUNT_OF(possible_scenes));
            }
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_learn_enter_name_on_exit(void* context) {
    InfraredApp* infrared = context;
    UNUSED(infrared);
}
