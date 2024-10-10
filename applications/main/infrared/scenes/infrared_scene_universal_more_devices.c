#include "../infrared_app_i.h"
#include <storage/storage.h>
#include <furi.h>
#include <dolphin/dolphin.h>

static void
    infrared_scene_universal_more_devices_callback(void* context, int32_t index, InputType type) {
    UNUSED(type);
    InfraredApp* infrared = context;
    uint32_t event = infrared_custom_event_pack(InfraredCustomEventTypeButtonSelected, index);
    view_dispatcher_send_custom_event(infrared->view_dispatcher, event);
}

static void infrared_scene_universal_more_devices_progress_back_callback(void* context) {
    InfraredApp* infrared = context;
    uint32_t event = infrared_custom_event_pack(InfraredCustomEventTypeBackPressed, -1);
    view_dispatcher_send_custom_event(infrared->view_dispatcher, event);
}

static void
    infrared_scene_universal_more_devices_show_popup(InfraredApp* infrared, uint32_t record_count) {
    ViewStack* view_stack = infrared->view_stack;
    InfraredProgressView* progress = infrared->progress;
    infrared_progress_view_set_progress_total(progress, record_count);
    infrared_progress_view_set_back_callback(
        progress, infrared_scene_universal_more_devices_progress_back_callback, infrared);
    view_stack_add_view(view_stack, infrared_progress_view_get_view(progress));
    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStartSend);
}

static void infrared_scene_universal_more_devices_hide_popup(InfraredApp* infrared) {
    ViewStack* view_stack = infrared->view_stack;
    InfraredProgressView* progress = infrared->progress;
    view_stack_remove_view(view_stack, infrared_progress_view_get_view(progress));
    infrared_play_notification_message(infrared, InfraredNotificationMessageBlinkStop);
}

static int32_t infrared_scene_universal_more_devices_task_callback(void* context) {
    InfraredApp* infrared = context;
    const InfraredErrorCode error = infrared_brute_force_calculate_messages(infrared->brute_force);
    view_dispatcher_send_custom_event(
        infrared->view_dispatcher,
        infrared_custom_event_pack(InfraredCustomEventTypeTaskFinished, 0));

    return error;
}

void infrared_scene_universal_more_devices_on_enter(void* context) {
    // in this func, note that it's actually loaded the db twice, here's what i did and why it's harmless:
    // 1. load db previously cuz need to use the db to add btns in runtime
    // 2. load db again with blocking task
    // reason:
    // 1. there's a funny policy in infrared_brute_force_calculate_messages func, that it'll check if name in it, if yes, it will increase the num and add, if yes, it will make a new field.
    // it's probablt a work around to satisfying other places, but since momentum is not a distro, i really don't want to edit that func to make it's hard to merge upstream changes.
    // why it's harmless:
    // 1. do it twice can make it cover the first custom item.
    // 2. only the count (which controls the for loop to go through the next item by name field) x2, not index, so signal index not impacted.
    InfraredApp* infrared = context;
    ButtonMenu* button_menu = infrared->button_menu;
    InfraredBruteForce* brute_force = infrared->brute_force;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, INFRARED_APP_EXTENSION, &I_ir_10px);
    browser_options.base_path = INFRARED_APP_FOLDER;

    bool file_selected = dialog_file_browser_show(
        infrared->dialogs, infrared->file_path, infrared->file_path, &browser_options);

    if(file_selected) {
        infrared_brute_force_set_db_filename(
            brute_force, furi_string_get_cstr(infrared->file_path));

        // load db previously cuz need to use the db to add btns in runtime
        InfraredErrorCode error = infrared_brute_force_calculate_messages(brute_force);

        if(INFRARED_ERROR_PRESENT(error)) {
            infrared_show_error_message(infrared, "Failed to load database");
            scene_manager_previous_scene(infrared->scene_manager);
            return;
        }

        // add btns
        for(size_t i = 0; i < infrared_brute_force_get_db_size(brute_force); ++i) {
            const char* button_name = infrared_brute_force_get_button_name(brute_force, i);
            button_menu_add_item(
                button_menu,
                button_name,
                i,
                infrared_scene_universal_more_devices_callback,
                ButtonMenuItemTypeCommon,
                infrared);
        }

        ///header name handler
        const char* file_name = strrchr(furi_string_get_cstr(infrared->file_path), '/');
        if(file_name) {
            file_name++; // skip dir seperator
        } else {
            file_name = furi_string_get_cstr(infrared->file_path); // fallback
        }
        button_menu_set_header(button_menu, file_name);

        view_set_orientation(view_stack_get_view(infrared->view_stack), ViewOrientationVertical);
        view_stack_add_view(infrared->view_stack, button_menu_get_view(infrared->button_menu));

        // Load universal remote data in background
        infrared_blocking_task_start(
            infrared, infrared_scene_universal_more_devices_task_callback);
        view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewStack);
    } else {
        scene_manager_previous_scene(infrared->scene_manager);
    }
}

bool infrared_scene_universal_more_devices_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    SceneManager* scene_manager = infrared->scene_manager;
    InfraredBruteForce* brute_force = infrared->brute_force;
    bool consumed = false;

    if(infrared_brute_force_is_started(brute_force)) {
        if(event.type == SceneManagerEventTypeTick) {
            bool success = infrared_brute_force_send_next(brute_force);
            if(success) {
                success = infrared_progress_view_increase_progress(infrared->progress);
            }
            if(!success) {
                infrared_brute_force_stop(brute_force);
                infrared_scene_universal_more_devices_hide_popup(infrared);
            }
            consumed = true;
        } else if(event.type == SceneManagerEventTypeCustom) {
            if(infrared_custom_event_get_type(event.event) == InfraredCustomEventTypeBackPressed) {
                infrared_brute_force_stop(brute_force);
                infrared_scene_universal_more_devices_hide_popup(infrared);
            }
            consumed = true;
        }
    } else {
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_previous_scene(scene_manager);
            consumed = true;
        } else if(event.type == SceneManagerEventTypeCustom) {
            uint16_t event_type;
            int16_t event_value;
            infrared_custom_event_unpack(event.event, &event_type, &event_value);

            if(event_type == InfraredCustomEventTypeButtonSelected) {
                uint32_t record_count;
                if(infrared_brute_force_start(brute_force, event_value, &record_count)) {
                    dolphin_deed(DolphinDeedIrSend);
                    infrared_scene_universal_more_devices_show_popup(infrared, record_count);
                } else {
                    scene_manager_next_scene(scene_manager, InfraredSceneErrorDatabases);
                }
            } else if(event_type == InfraredCustomEventTypeTaskFinished) {
                const InfraredErrorCode task_error = infrared_blocking_task_finalize(infrared);

                if(INFRARED_ERROR_PRESENT(task_error)) {
                    scene_manager_next_scene(infrared->scene_manager, InfraredSceneErrorDatabases);
                } else {
                    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewStack);
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_universal_more_devices_on_exit(void* context) {
    InfraredApp* infrared = context;
    ButtonMenu* button_menu = infrared->button_menu;
    view_stack_remove_view(infrared->view_stack, button_menu_get_view(button_menu));
    button_menu_reset(button_menu);
    infrared_brute_force_reset(infrared->brute_force);
}
