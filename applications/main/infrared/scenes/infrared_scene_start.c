#include "../infrared_app_i.h"

enum SubmenuIndex {
    SubmenuIndexUniversalRemotes,
    SubmenuIndexLearnNewRemote,
    SubmenuIndexSavedRemotes,
    SubmenuIndexGpioSettings,
    SubmenuIndexLearnNewRemoteRaw,
    SubmenuIndexDebug,
    SubmenuIndexEasyLearn,
};

static void infrared_scene_start_submenu_callback(void* context, uint32_t index) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, index);
}

void infrared_scene_start_on_enter(void* context) {
    InfraredApp* infrared = context;
    Submenu* submenu = infrared->submenu;
    SceneManager* scene_manager = infrared->scene_manager;

    submenu_add_item(
        submenu,
        "Universal Remotes",
        SubmenuIndexUniversalRemotes,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Learn New Remote",
        SubmenuIndexLearnNewRemote,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Saved Remotes",
        SubmenuIndexSavedRemotes,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "GPIO Settings",
        SubmenuIndexGpioSettings,
        infrared_scene_start_submenu_callback,
        infrared);

    char easy_learn_text[24];
    snprintf(
        easy_learn_text,
        sizeof(easy_learn_text),
        "Easy Learn [%s]",
        infrared->app_state.is_easy_mode ? "X" : " ");
    submenu_add_item(
        submenu,
        easy_learn_text,
        SubmenuIndexEasyLearn,
        infrared_scene_start_submenu_callback,
        infrared);

    submenu_add_lockable_item(
        submenu,
        "Learn New Remote RAW",
        SubmenuIndexLearnNewRemoteRaw,
        infrared_scene_start_submenu_callback,
        infrared,
        !infrared->app_state.is_debug_enabled,
        "Enable\n"
        "Settings >\n"
        "System >\n"
        "Debug");
    submenu_add_lockable_item(
        submenu,
        "Debug RX",
        SubmenuIndexDebug,
        infrared_scene_start_submenu_callback,
        infrared,
        !infrared->app_state.is_debug_enabled,
        "Enable\n"
        "Settings >\n"
        "System >\n"
        "Debug");

    const uint32_t submenu_index =
        scene_manager_get_scene_state(scene_manager, InfraredSceneStart);
    submenu_set_selected_item(submenu, submenu_index);

    // Only reset menu position if we're not coming from a toggle
    if(submenu_index == 0) {
        scene_manager_set_scene_state(
            scene_manager, InfraredSceneStart, SubmenuIndexUniversalRemotes);
    }

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewSubmenu);
}

bool infrared_scene_start_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    SceneManager* scene_manager = infrared->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexEasyLearn) {
            infrared->app_state.is_easy_mode = !infrared->app_state.is_easy_mode;
            infrared_save_settings(infrared);
            // Save current position
            scene_manager_set_scene_state(
                scene_manager, InfraredSceneStart, SubmenuIndexEasyLearn);
            scene_manager_previous_scene(scene_manager);
            scene_manager_next_scene(scene_manager, InfraredSceneStart);
            consumed = true;
        } else {
            scene_manager_set_scene_state(scene_manager, InfraredSceneStart, event.event);
            if(event.event == SubmenuIndexUniversalRemotes) {
                furi_string_set(infrared->file_path, INFRARED_APP_FOLDER);
                scene_manager_next_scene(scene_manager, InfraredSceneUniversal);
            } else if(
                event.event == SubmenuIndexLearnNewRemote ||
                event.event == SubmenuIndexLearnNewRemoteRaw) {
                infrared_worker_rx_enable_signal_decoding(
                    infrared->worker, event.event == SubmenuIndexLearnNewRemote);
                infrared->app_state.is_learning_new_remote = true;
                scene_manager_next_scene(scene_manager, InfraredSceneLearn);
            } else if(event.event == SubmenuIndexSavedRemotes) {
                furi_string_set(infrared->file_path, INFRARED_APP_FOLDER);
                scene_manager_next_scene(scene_manager, InfraredSceneRemoteList);
            } else if(event.event == SubmenuIndexGpioSettings) {
                scene_manager_next_scene(scene_manager, InfraredSceneGpioSettings);
            } else if(event.event == SubmenuIndexDebug) {
                scene_manager_next_scene(scene_manager, InfraredSceneDebug);
            }
            consumed = true;
        }
    }
    return consumed;
}

void infrared_scene_start_on_exit(void* context) {
    InfraredApp* infrared = context;
    submenu_reset(infrared->submenu);
}
