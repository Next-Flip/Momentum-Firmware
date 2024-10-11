#include "../infrared_app_i.h"

#include "common/infrared_scene_universal_common.h"

#include <storage/storage.h>
#include <furi.h>
#include <dolphin/dolphin.h>

static void infrared_scene_universal_more_devices_item_callback(
    void* context,
    int32_t index,
    InputType type) {
    UNUSED(type);
    InfraredApp* infrared = context;
    uint32_t event = infrared_custom_event_pack(InfraredCustomEventTypeButtonSelected, index);
    view_dispatcher_send_custom_event(infrared->view_dispatcher, event);
}

static int32_t infrared_scene_universal_more_devices_task_callback(void* context) {
    InfraredApp* infrared = context;
    ButtonMenu* button_menu = infrared->button_menu;
    InfraredBruteForce* brute_force = infrared->brute_force;
    const InfraredErrorCode error =
        infrared_brute_force_calculate_messages(infrared->brute_force, true);

    if(!INFRARED_ERROR_PRESENT(error)) {
        // add btns
        for(size_t i = 0; i < infrared_brute_force_get_button_count(brute_force); ++i) {
            const char* button_name = infrared_brute_force_get_button_name(brute_force, i);
            button_menu_add_item(
                button_menu,
                button_name,
                i,
                infrared_scene_universal_more_devices_item_callback,
                ButtonMenuItemTypeCommon,
                infrared);
        }
    }

    view_dispatcher_send_custom_event(
        infrared->view_dispatcher,
        infrared_custom_event_pack(InfraredCustomEventTypeTaskFinished, 0));

    return error;
}

void infrared_scene_universal_more_devices_on_enter(void* context) {
    InfraredApp* infrared = context;
    ButtonMenu* button_menu = infrared->button_menu;
    InfraredBruteForce* brute_force = infrared->brute_force;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, INFRARED_APP_EXTENSION, &I_ir_10px);
    browser_options.base_path = INFRARED_APP_FOLDER;
    browser_options.skip_assets = false;
    if(!dialog_file_browser_show(
           infrared->dialogs, infrared->file_path, infrared->file_path, &browser_options)) {
        scene_manager_previous_scene(infrared->scene_manager);
        return;
    }

    infrared_brute_force_set_db_filename(brute_force, furi_string_get_cstr(infrared->file_path));

    // File name in header
    // Using c-string functions on FuriString is a bad idea but file_path is not modified
    // for the lifetime of this scene so it should be fine
    const char* file_name = strrchr(furi_string_get_cstr(infrared->file_path), '/');
    if(file_name) {
        file_name++; // skip dir seperator
    } else {
        file_name = furi_string_get_cstr(infrared->file_path); // fallback
    }
    button_menu_set_header(button_menu, file_name);

    // Can't use infrared_scene_universal_common_on_enter() since we use ButtonMenu not ButtonPanel
    view_set_orientation(view_stack_get_view(infrared->view_stack), ViewOrientationVertical);
    view_stack_add_view(infrared->view_stack, button_menu_get_view(infrared->button_menu));

    // Load universal remote data in background
    infrared_blocking_task_start(infrared, infrared_scene_universal_more_devices_task_callback);
}

bool infrared_scene_universal_more_devices_on_event(void* context, SceneManagerEvent event) {
    return infrared_scene_universal_common_on_event(context, event);
}

void infrared_scene_universal_more_devices_on_exit(void* context) {
    // Can't use infrared_scene_universal_common_on_exit() since we use ButtonMenu not ButtonPanel
    InfraredApp* infrared = context;
    ButtonMenu* button_menu = infrared->button_menu;
    view_stack_remove_view(infrared->view_stack, button_menu_get_view(button_menu));
    infrared_brute_force_reset(infrared->brute_force);
    button_menu_reset(button_menu);
}
