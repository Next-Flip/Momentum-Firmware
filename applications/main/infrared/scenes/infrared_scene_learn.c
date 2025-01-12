#include "../infrared_app_i.h"
#include <dolphin/dolphin.h>

static const char* const easy_mode_button_names[] = {"Power", "Vol_up", "Vol_dn", "Ch_up", "Ch_dn",
                                                     "Mute",  "Eject",  "Input",  "Back",  "Ok",
                                                     "Up",    "Down",   "Left",   "Right", "Play",
                                                     "Pause", "Stop",   "Prev",   "Next",  "Rew",
                                                     "FF",    "Exit",   "Menu"};

void infrared_scene_learn_on_enter(void* context) {
    InfraredApp* infrared = context;
    Popup* popup = infrared->popup;
    InfraredWorker* worker = infrared->worker;

    infrared_worker_rx_set_received_signal_callback(
        worker, infrared_signal_received_callback, context);
    infrared_worker_rx_start(worker);
    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStartRead);

    popup_set_icon(popup, 0, 32, &I_InfraredLearnShort_128x31);
    popup_set_header(popup, NULL, 0, 0, AlignCenter, AlignCenter);

    if(infrared->app_state.is_easy_mode) {
        size_t button_count = 0;
        if(!infrared->app_state.is_learning_new_remote && infrared->remote) {
            button_count = infrared_remote_get_signal_count(infrared->remote);
        }
        const char* button_name = button_count < COUNT_OF(easy_mode_button_names) ?
                                      easy_mode_button_names[button_count] :
                                      "Button";

        infrared_text_store_set(
            infrared, 0, "Point remote at IR port\nand press the %s button", button_name);
        popup_set_text(popup, infrared->text_store[0], 5, 10, AlignLeft, AlignCenter);
    } else {
        popup_set_text(
            popup,
            "Point the remote at IR port\nand push the button",
            5,
            10,
            AlignLeft,
            AlignCenter);
    }

    popup_set_callback(popup, NULL);
    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewPopup);
}

bool infrared_scene_learn_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InfraredCustomEventTypeSignalReceived) {
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneLearnSuccess);
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_learn_on_exit(void* context) {
    InfraredApp* infrared = context;
    popup_set_header(infrared->popup, NULL, 0, 0, AlignCenter, AlignCenter);
    popup_set_text(infrared->popup, NULL, 0, 0, AlignCenter, AlignTop);
    popup_set_icon(infrared->popup, 0, 0, NULL);

    infrared_worker_rx_stop(infrared->worker);
    infrared_worker_rx_set_received_signal_callback(infrared->worker, NULL, NULL);

    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStop);
}
