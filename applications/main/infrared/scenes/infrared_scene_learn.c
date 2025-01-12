#include "../infrared_app_i.h"
#include <dolphin/dolphin.h>

static const char* const easy_mode_button_names[] = {"Power", "Vol_up", "Vol_dn", "Ch_up", "Ch_dn",
                                                     "Mute",  "Eject",  "Input",  "Back",  "Ok",
                                                     "Up",    "Down",   "Left",   "Right", "Play",
                                                     "Pause", "Stop",   "Prev",   "Next",  "Rew",
                                                     "FF",    "Exit",   "Menu"};

static void infrared_scene_learn_dialog_result_callback(DialogExResult result, void* context) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, result);
}

static void infrared_scene_learn_update_button_name(InfraredApp* infrared, bool increment) {
    DialogEx* dialog_ex = infrared->dialog_ex;
    int32_t button_index;

    if(infrared->app_state.is_learning_new_remote) {
        // For new remotes, use current_button_index directly
        button_index = infrared->app_state.current_button_index;
        if(increment) {
            // Only increment if we haven't reached the last button
            if(button_index + 1 < (int32_t)COUNT_OF(easy_mode_button_names)) {
                button_index++;
                infrared->app_state.current_button_index = button_index;
            }
        }
    } else if(infrared->remote) {
        // For existing remotes, get signal count but ensure it fits in int32_t
        button_index = (int32_t)infrared_remote_get_signal_count(infrared->remote);
        if(increment && button_index + 1 < (int32_t)COUNT_OF(easy_mode_button_names)) {
            button_index++;
        }
    } else {
        button_index = 0;
    }

    // Ensure button_index is valid
    if(button_index < 0) button_index = 0;
    if(button_index >= (int32_t)COUNT_OF(easy_mode_button_names)) {
        button_index = (int32_t)COUNT_OF(easy_mode_button_names) - 1;
    }

    // Now we know button_index is valid, use it to get the name
    const char* button_name = easy_mode_button_names[button_index];

    infrared_text_store_set(
        infrared, 0, "Point remote at IR port\nand press the %s button", button_name);
    dialog_ex_set_text(dialog_ex, infrared->text_store[0], 5, 10, AlignLeft, AlignCenter);

    // Hide skip button if we're at the last predefined button
    if(button_index + 1 >= (int32_t)COUNT_OF(easy_mode_button_names)) {
        dialog_ex_set_center_button_text(dialog_ex, NULL);
    }
}

void infrared_scene_learn_on_enter(void* context) {
    InfraredApp* infrared = context;
    DialogEx* dialog_ex = infrared->dialog_ex;
    InfraredWorker* worker = infrared->worker;

    // Initialize or validate current_button_index
    if(infrared->app_state.is_learning_new_remote) {
        // If index is beyond our predefined names, reset it
        if(infrared->app_state.current_button_index >= (int32_t)COUNT_OF(easy_mode_button_names)) {
            infrared->app_state.current_button_index = 0;
        }
    }

    infrared_worker_rx_set_received_signal_callback(
        worker, infrared_signal_received_callback, context);
    infrared_worker_rx_start(worker);
    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStartRead);

    dialog_ex_set_icon(dialog_ex, 0, 32, &I_InfraredLearnShort_128x31);
    dialog_ex_set_header(dialog_ex, NULL, 0, 0, AlignCenter, AlignCenter);

    if(infrared->app_state.is_easy_mode) {
        infrared_scene_learn_update_button_name(infrared, false);
        dialog_ex_set_icon(dialog_ex, 0, 22, &I_InfraredLearnShort_128x31);
        // Only show skip if not at last button
        if(infrared->app_state.current_button_index + 1 <
           (int32_t)COUNT_OF(easy_mode_button_names)) {
            dialog_ex_set_center_button_text(dialog_ex, "Skip");
        }
    } else {
        dialog_ex_set_text(
            dialog_ex,
            "Point the remote at IR port\nand push the button",
            5,
            13,
            AlignLeft,
            AlignCenter);
    }

    dialog_ex_set_context(dialog_ex, context);
    dialog_ex_set_result_callback(dialog_ex, infrared_scene_learn_dialog_result_callback);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewDialogEx);
}

bool infrared_scene_learn_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InfraredCustomEventTypeSignalReceived) {
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneLearnSuccess);
            consumed = true;
        } else if(event.event == DialogExResultCenter && infrared->app_state.is_easy_mode) {
            // Update with increment when skipping
            infrared_scene_learn_update_button_name(infrared, true);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Reset button index when exiting learn mode completely
        if(infrared->app_state.is_learning_new_remote) {
            infrared->app_state.current_button_index = 0;
        }
        consumed = false;
    }

    return consumed;
}

void infrared_scene_learn_on_exit(void* context) {
    InfraredApp* infrared = context;

    // Reset dialog
    dialog_ex_reset(infrared->dialog_ex);

    // Stop worker and clear callback
    infrared_worker_rx_stop(infrared->worker);
    infrared_worker_rx_set_received_signal_callback(infrared->worker, NULL, NULL);

    // Clear any stored text
    infrared_text_store_clear(infrared, 0);

    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStop);
}
