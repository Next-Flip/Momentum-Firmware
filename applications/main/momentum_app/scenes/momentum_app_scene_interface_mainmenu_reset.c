#include "../momentum_app.h"

static void momentum_app_scene_interface_mainmenu_reset_dialog_callback(
    DialogExResult result,
    void* context) {
    MomentumApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void momentum_app_scene_interface_mainmenu_reset_on_enter(void* context) {
    MomentumApp* app = context;
    DialogEx* dialog_ex = app->dialog_ex;

    dialog_ex_set_header(dialog_ex, "Reset Menu Apps?", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(dialog_ex, "Your edits will be lost!", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog_ex, "Cancel");
    dialog_ex_set_right_button_text(dialog_ex, "Reset");

    dialog_ex_set_context(dialog_ex, app);
    dialog_ex_set_result_callback(
        dialog_ex, momentum_app_scene_interface_mainmenu_reset_dialog_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewDialogEx);
}

bool momentum_app_scene_interface_mainmenu_reset_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultRight:
            bool reset = false;
            Stream* stream = file_stream_alloc(furi_record_open(RECORD_STORAGE));
            if(file_stream_open(stream, MAINMENU_APPS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                stream_write_format(stream, "MenuAppList Version %u\n", 1);
                for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
                    stream_write_format(stream, "%s\n", FLIPPER_APPS[i].name);
                }
                for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT - 1; i++) {
                    stream_write_format(stream, "%s\n", FLIPPER_EXTERNAL_APPS[i].name);
                }
                reset = true;
            }
            file_stream_close(stream);
            stream_free(stream);
            furi_record_close(RECORD_STORAGE);
            if(reset) {
                app->save_mainmenu_apps = false;
                app->require_reboot = true;
                momentum_app_apply(app);
            }
            break;
        case DialogExResultLeft:
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }

    return consumed;
}

void momentum_app_scene_interface_mainmenu_reset_on_exit(void* context) {
    MomentumApp* app = context;
    DialogEx* dialog_ex = app->dialog_ex;

    dialog_ex_reset(dialog_ex);
}
