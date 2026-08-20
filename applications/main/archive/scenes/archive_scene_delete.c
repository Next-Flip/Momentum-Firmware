#include "../archive_i.h"
#include "../helpers/archive_apps.h"
#include "../helpers/archive_browser.h"

#define SCENE_DELETE_CUSTOM_EVENT (0UL)

void archive_scene_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void archive_scene_delete_on_enter(void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    ArchiveBrowserView* browser = app->browser;

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", archive_scene_delete_widget_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Delete", archive_scene_delete_widget_callback, app);

    char delete_str[64];

    with_view_model(
        browser->view,
        ArchiveBrowserViewModel * model,
        {
            if(model->select_mode && model->selected_count > 1) {
                snprintf(
                    delete_str,
                    sizeof(delete_str),
                    "\e#Delete %d files?\e#",
                    model->selected_count);
                widget_add_file_list_element(
                    app->widget,
                    0,
                    23,
                    3,
                    model->selected_files,
                    model->selected_count,
                    14,
                    FRAME_HEIGHT * 3,
                    false);
            } else {
                ArchiveFile_t* current = archive_get_current_file(browser);
                FuriString* filename = furi_string_alloc();
                path_extract_filename(current->path, filename, false);
                snprintf(
                    delete_str,
                    sizeof(delete_str),
                    "\e#Delete %s?\e#",
                    furi_string_get_cstr(filename));
                furi_string_free(filename);
            }
        },
        false);

    widget_add_text_box_element(
        app->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, delete_str, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewWidget);
}

bool archive_scene_delete_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    ArchiveBrowserView* browser = app->browser;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            // Show loading popup on delete
            view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewStack);
            archive_show_loading_popup(app, true);

            with_view_model(
                browser->view,
                ArchiveBrowserViewModel * model,
                {
                    if(model->select_mode && model->selected_count > 0) {
                        for(size_t i = 0; i < model->selected_count; i++) {
                            archive_delete_file(
                                browser, "%s", furi_string_get_cstr(model->selected_files[i]));
                        }
                        archive_clear_selection(model);
                    } else {
                        ArchiveFile_t* selected = archive_get_current_file(browser);
                        const char* name = archive_get_name(browser);
                        if(selected->is_app) {
                            archive_app_delete_file(browser, name);
                        } else {
                            archive_delete_file(browser, "%s", name);
                        }
                    }
                },
                false);

            archive_show_loading_popup(app, false);
            return scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == GuiButtonTypeLeft) {
            return scene_manager_previous_scene(app->scene_manager);
        }
    }
    return false;
}

void archive_scene_delete_on_exit(void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;

    widget_reset(app->widget);
}
