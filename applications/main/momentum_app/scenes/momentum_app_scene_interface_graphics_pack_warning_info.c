#include "../momentum_app.h"

void momentum_app_scene_interface_graphics_pack_warning_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MomentumApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void momentum_app_scene_interface_graphics_pack_warning_info_on_enter(void* context) {
    MomentumApp* app = context;

    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Exit",
        momentum_app_scene_interface_graphics_pack_warning_info_widget_callback,
        app);

    const char* dirs[] = {"Fonts", "Icons"};
    const char* font_exts[] = {".u8f"};
    const char* icon_exts[] = {".bm", ".bmx"};
    const char** exts[] = {font_exts, icon_exts};
    const size_t ext_counts[] = {COUNT_OF(font_exts), COUNT_OF(icon_exts)};
    const char* selected_pack =
        app->asset_pack_index == 0 ?
            "Default" :
            *CharList_get(app->asset_pack_names, app->asset_pack_index - 1);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc();
    FuriString** paths = NULL;
    size_t num_paths = 0;

    for(size_t i = 0; i < COUNT_OF(dirs); i++) {
        FuriString** files = NULL;
        size_t num_files = 0;

        furi_string_printf(path, "%s/%s/%s", ASSET_PACKS_PATH, selected_pack, dirs[i]);
        if(storage_dir_exists(storage, furi_string_get_cstr(path))) {
            if(storage_list_dir(
                   storage,
                   furi_string_get_cstr(path),
                   &files,
                   &num_files,
                   true,
                   exts[i],
                   ext_counts[i])) {
                paths = realloc(paths, (num_paths + num_files) * sizeof(FuriString*));
                memcpy(&paths[num_paths], files, num_files * sizeof(FuriString*));
                num_paths += num_files;
                free(files);
            }
        }
    }

    size_t f_count = 0;
    size_t i_count = 0;
    for(size_t i = 0; i < num_paths; i++) {
        if(furi_string_start_with_str(paths[i], ASSET_PACKS_PATH)) {
            if(furi_string_search_str(paths[i], "/Fonts/") != FURI_STRING_FAILURE) {
                f_count++;
            } else if(furi_string_search_str(paths[i], "/Icons/") != FURI_STRING_FAILURE) {
                i_count++;
            }
        }
    }

    FuriString* title = furi_string_alloc();
    if(f_count > 0 && i_count > 0) {
        furi_string_printf(title, "%zu Fonts, %zu Icons", f_count, i_count);
    } else if(f_count > 0) {
        furi_string_printf(title, "%zu Fonts", f_count);
    } else if(i_count > 0) {
        furi_string_printf(title, "%zu Icons", i_count);
    }

    widget_add_string_element(
        app->widget, 35, 57, AlignLeft, AlignCenter, FontPrimary, furi_string_get_cstr(title));
    furi_string_free(title);

    widget_add_file_list_element(app->widget, 0, 12, 4, paths, num_paths, 0, 52, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewWidget);
}

bool momentum_app_scene_interface_graphics_pack_warning_info_on_event(
    void* context,
    SceneManagerEvent event) {
    MomentumApp* app = context;

    if((event.type == SceneManagerEventTypeCustom && event.event == GuiButtonTypeLeft) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MomentumAppSceneInterfaceGraphics);
        return true;
    }
    return false;
}

void momentum_app_scene_interface_graphics_pack_warning_info_on_exit(void* context) {
    MomentumApp* app = context;
    widget_reset(app->widget);
}
