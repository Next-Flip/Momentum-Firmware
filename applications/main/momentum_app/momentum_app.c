#include "momentum_app.h"

static bool momentum_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MomentumApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void callback_reboot(void* context) {
    UNUSED(context);
    Power* power = furi_record_open(RECORD_POWER);
    power_reboot(power, PowerBootModeNormal);
}

bool momentum_app_apply(MomentumApp* app) {
    if(app->save_mainmenu_apps) {
        Stream* stream = file_stream_alloc(app->storage);
        if(file_stream_open(stream, MAINMENU_APPS_PATH, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
            stream_write_format(stream, "MenuAppList Version %u\n", 1);
            for(size_t i = 0; i < CharList_size(app->mainmenu_app_exes); i++) {
                stream_write_format(stream, "%s\n", *CharList_get(app->mainmenu_app_exes, i));
            }
        }
        file_stream_close(stream);
        stream_free(stream);
    }

    if(app->save_desktop) {
        desktop_api_set_settings(app->desktop, &app->desktop_settings);
    }

    if(app->save_subghz_freqs) {
        FlipperFormat* file = flipper_format_file_alloc(app->storage);
        do {
            FrequencyList_it_t it;
            if(!flipper_format_file_open_always(file, EXT_PATH("subghz/assets/setting_user")))
                break;

            if(!flipper_format_write_header_cstr(
                   file, SUBGHZ_SETTING_FILE_TYPE, SUBGHZ_SETTING_FILE_VERSION))
                break;

            while(flipper_format_delete_key(file, "Add_standard_frequencies"))
                ;
            flipper_format_write_bool(
                file, "Add_standard_frequencies", &app->subghz_use_defaults, 1);

            if(!flipper_format_rewind(file)) break;
            while(flipper_format_delete_key(file, "Frequency"))
                ;
            FrequencyList_it(it, app->subghz_static_freqs);
            for(size_t i = 0; i < FrequencyList_size(app->subghz_static_freqs); i++) {
                flipper_format_write_uint32(
                    file, "Frequency", FrequencyList_get(app->subghz_static_freqs, i), 1);
            }

            if(!flipper_format_rewind(file)) break;
            while(flipper_format_delete_key(file, "Hopper_frequency"))
                ;
            for(size_t i = 0; i < FrequencyList_size(app->subghz_hopper_freqs); i++) {
                flipper_format_write_uint32(
                    file, "Hopper_frequency", FrequencyList_get(app->subghz_hopper_freqs, i), 1);
            }
        } while(false);
        flipper_format_free(file);
    }

    if(app->save_subghz) {
        FlipperFormat* file = flipper_format_file_alloc(app->storage);
        do {
            if(!flipper_format_file_open_always(file, "/ext/subghz/assets/extend_range.txt"))
                break;
            if(!flipper_format_write_header_cstr(file, "Flipper SubGhz Setting File", 1)) break;
            if(!flipper_format_write_comment_cstr(
                   file, "Whether to allow extended ranges that can break your flipper"))
                break;
            if(!flipper_format_write_bool(
                   file, "use_ext_range_at_own_risk", &app->subghz_extend, 1))
                break;
            if(!flipper_format_write_bool(file, "ignore_default_tx_region", &app->subghz_bypass, 1))
                break;
        } while(0);
        flipper_format_free(file);
    }

    if(app->save_name) {
        if(strcmp(app->device_name, "") == 0) {
            storage_simply_remove(app->storage, NAMESPOOF_PATH);
        } else {
            FlipperFormat* file = flipper_format_file_alloc(app->storage);

            do {
                if(!flipper_format_file_open_always(file, NAMESPOOF_PATH)) break;
                if(!flipper_format_write_header_cstr(file, NAMESPOOF_HEADER, NAMESPOOF_VERSION))
                    break;
                if(!flipper_format_write_string_cstr(file, "Name", app->device_name)) break;
            } while(0);

            flipper_format_free(file);
        }
    }

    if(app->save_dolphin) {
        if(app->save_xp) {
            app->dolphin->state->data.icounter = app->dolphin_xp;
        }
        if(app->save_angry) {
            app->dolphin->state->data.butthurt = app->dolphin_angry;
        }
        app->dolphin->state->dirty = true;
        dolphin_flush(app->dolphin);
        dolphin_reload_state(app->dolphin);
    }

    if(app->save_backlight) {
        rgb_backlight_save_settings();
    }

    if(app->save_settings) {
        momentum_settings_save();
    }

    if(app->show_slideshow) {
        callback_reboot(NULL);
    } else if(app->require_reboot) {
        popup_set_header(app->popup, "Rebooting...", 64, 26, AlignCenter, AlignCenter);
        popup_set_text(app->popup, "Applying changes...", 64, 40, AlignCenter, AlignCenter);
        popup_set_callback(app->popup, callback_reboot);
        popup_set_context(app->popup, app);
        popup_set_timeout(app->popup, 1000);
        popup_enable_timeout(app->popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewPopup);
        return true;
    } else if(app->apply_pack) {
        asset_packs_free();
        popup_set_header(app->popup, "Reloading...", 64, 26, AlignCenter, AlignCenter);
        popup_set_text(app->popup, "Applying asset pack...", 64, 40, AlignCenter, AlignCenter);
        popup_set_callback(app->popup, NULL);
        popup_set_context(app->popup, NULL);
        popup_set_timeout(app->popup, 0);
        popup_disable_timeout(app->popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewPopup);
        asset_packs_init();
    }
    return false;
}

static bool momentum_app_back_event_callback(void* context) {
    furi_assert(context);
    MomentumApp* app = context;

    if(!scene_manager_has_previous_scene(app->scene_manager, MomentumAppSceneStart)) {
        if(momentum_app_apply(app)) {
            return true;
        }
    }

    return scene_manager_handle_back_event(app->scene_manager);
}

static void momentum_app_push_mainmenu_app(MomentumApp* app, FuriString* label, FuriString* exe) {
    CharList_push_back(app->mainmenu_app_exes, strdup(furi_string_get_cstr(exe)));
    // Display logic mimics applications/services/gui/modules/menu.c
    if(furi_string_equal(label, "Momentum")) {
        furi_string_set(label, "MNTM");
    } else if(furi_string_equal(label, "125 kHz RFID")) {
        furi_string_set(label, "RFID");
    } else if(furi_string_equal(label, "Sub-GHz")) {
        furi_string_set(label, "SubGHz");
    } else if(furi_string_start_with_str(label, "[")) {
        size_t trim = furi_string_search_str(label, "] ", 1);
        if(trim != FURI_STRING_FAILURE) {
            furi_string_right(label, trim + 2);
        }
    }
    CharList_push_back(app->mainmenu_app_labels, strdup(furi_string_get_cstr(label)));
}

void momentum_app_load_mainmenu_apps(MomentumApp* app) {
    // Loading logic mimics applications/services/loader/loader_menu.c
    Stream* stream = file_stream_alloc(app->storage);
    FuriString* line = furi_string_alloc();
    FuriString* label = furi_string_alloc();
    uint32_t version;
    uint8_t* unused_icon = malloc(FAP_MANIFEST_MAX_ICON_SIZE);
    if(file_stream_open(stream, MAINMENU_APPS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       stream_read_line(stream, line) &&
       sscanf(furi_string_get_cstr(line), "MenuAppList Version %lu", &version) == 1 &&
       version <= 1) {
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(version == 0) {
                if(furi_string_equal(line, "RFID")) {
                    furi_string_set(line, "125 kHz RFID");
                } else if(furi_string_equal(line, "SubGHz")) {
                    furi_string_set(line, "Sub-GHz");
                } else if(furi_string_equal(line, "Xtreme")) {
                    furi_string_set(line, "Momentum");
                }
            }
            if(furi_string_start_with(line, "/")) {
                if(!flipper_application_load_name_and_icon(
                       line, app->storage, &unused_icon, label)) {
                    furi_string_reset(label);
                }
            } else {
                furi_string_reset(label);
                bool found = false;
                for(size_t i = 0; !found && i < FLIPPER_APPS_COUNT; i++) {
                    if(!strcmp(furi_string_get_cstr(line), FLIPPER_APPS[i].name)) {
                        furi_string_set(label, FLIPPER_APPS[i].name);
                        found = true;
                    }
                }
                for(size_t i = 0; !found && i < FLIPPER_EXTERNAL_APPS_COUNT; i++) {
                    if(!strcmp(furi_string_get_cstr(line), FLIPPER_EXTERNAL_APPS[i].name)) {
                        furi_string_set(label, FLIPPER_EXTERNAL_APPS[i].name);
                        found = true;
                    }
                }
            }
            if(furi_string_empty(label)) {
                // Ignore unknown apps just like in main menu, prevents "ghost" apps when saving
                continue;
            }
            momentum_app_push_mainmenu_app(app, label, line);
        }
    } else {
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            furi_string_set(label, FLIPPER_APPS[i].name);
            furi_string_set(line, FLIPPER_APPS[i].name);
            momentum_app_push_mainmenu_app(app, label, line);
        }
        // Until count - 1 because last app is hardcoded below
        for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT - 1; i++) {
            furi_string_set(label, FLIPPER_EXTERNAL_APPS[i].name);
            furi_string_set(line, FLIPPER_EXTERNAL_APPS[i].name);
            momentum_app_push_mainmenu_app(app, label, line);
        }
    }
    free(unused_icon);
    furi_string_free(label);
    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
}

void momentum_app_empty_mainmenu_apps(MomentumApp* app) {
    CharList_it_t it;
    for(CharList_it(it, app->mainmenu_app_labels); !CharList_end_p(it); CharList_next(it)) {
        free(*CharList_cref(it));
    }
    CharList_reset(app->mainmenu_app_labels);
    for(CharList_it(it, app->mainmenu_app_exes); !CharList_end_p(it); CharList_next(it)) {
        free(*CharList_cref(it));
    }
    CharList_reset(app->mainmenu_app_exes);
}

MomentumApp* momentum_app_alloc() {
    MomentumApp* app = malloc(sizeof(MomentumApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->desktop = furi_record_open(RECORD_DESKTOP);
    app->dolphin = furi_record_open(RECORD_DOLPHIN);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->expansion = furi_record_open(RECORD_EXPANSION);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // View Dispatcher and Scene Manager
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&momentum_app_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, momentum_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, momentum_app_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Gui Modules
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MomentumAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MomentumAppViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MomentumAppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MomentumAppViewByteInput, byte_input_get_view(app->byte_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MomentumAppViewNumberInput,
        number_input_get_view(app->number_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MomentumAppViewPopup, popup_get_view(app->popup));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MomentumAppViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    // Settings init

    app->asset_pack_index = 0;
    CharList_init(app->asset_pack_names);
    File* folder = storage_file_alloc(app->storage);
    FileInfo info;
    char* name = malloc(ASSET_PACKS_NAME_LEN);
    if(storage_dir_open(folder, ASSET_PACKS_PATH)) {
        while(storage_dir_read(folder, &info, name, ASSET_PACKS_NAME_LEN)) {
            if(info.flags & FSF_DIRECTORY && name[0] != '.') {
                char* copy = strdup(name);
                size_t idx = 0;
                for(; idx < CharList_size(app->asset_pack_names); idx++) {
                    char* comp = *CharList_get(app->asset_pack_names, idx);
                    if(strcasecmp(copy, comp) < 0) {
                        break;
                    }
                }
                CharList_push_at(app->asset_pack_names, idx, copy);
                if(app->asset_pack_index != 0) {
                    if(idx < app->asset_pack_index) app->asset_pack_index++;
                } else {
                    if(strcmp(copy, momentum_settings.asset_pack) == 0)
                        app->asset_pack_index = idx + 1;
                }
            }
        }
    }
    free(name);
    storage_file_free(folder);

    CharList_init(app->mainmenu_app_labels);
    CharList_init(app->mainmenu_app_exes);
    momentum_app_load_mainmenu_apps(app);

    desktop_api_get_settings(app->desktop, &app->desktop_settings);

    FlipperFormat* file = flipper_format_file_alloc(app->storage);
    FrequencyList_init(app->subghz_static_freqs);
    FrequencyList_init(app->subghz_hopper_freqs);
    app->subghz_use_defaults = true;
    do {
        uint32_t temp;
        if(!flipper_format_file_open_existing(file, EXT_PATH("subghz/assets/setting_user"))) break;

        flipper_format_read_bool(file, "Add_standard_frequencies", &app->subghz_use_defaults, 1);

        if(!flipper_format_rewind(file)) break;
        while(flipper_format_read_uint32(file, "Frequency", &temp, 1)) {
            if(furi_hal_subghz_is_frequency_valid(temp)) {
                FrequencyList_push_back(app->subghz_static_freqs, temp);
            }
        }

        if(!flipper_format_rewind(file)) break;
        while(flipper_format_read_uint32(file, "Hopper_frequency", &temp, 1)) {
            if(furi_hal_subghz_is_frequency_valid(temp)) {
                FrequencyList_push_back(app->subghz_hopper_freqs, temp);
            }
        }
    } while(false);
    flipper_format_free(file);

    file = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_existing(file, "/ext/subghz/assets/extend_range.txt")) {
        flipper_format_read_bool(file, "use_ext_range_at_own_risk", &app->subghz_extend, 1);
        flipper_format_read_bool(file, "ignore_default_tx_region", &app->subghz_bypass, 1);
    }
    flipper_format_free(file);

    strlcpy(app->device_name, furi_hal_version_get_name_ptr(), FURI_HAL_VERSION_ARRAY_NAME_LENGTH);

    DolphinStats stats = dolphin_stats(app->dolphin);
    app->dolphin_xp = stats.icounter;
    app->dolphin_angry = stats.butthurt;

    // Will be "(version) (commit or date)"
    app->version_tag = furi_string_alloc_set(version_get_version(NULL));
    size_t separator = furi_string_size(app->version_tag);
    // Need canvas to calculate text length
    Canvas* canvas = gui_direct_draw_acquire(app->gui);
    canvas_set_font(canvas, FontPrimary);
    if(furi_string_equal(app->version_tag, "mntm-dev")) {
        // Add space, add commit sha
        furi_string_cat_printf(app->version_tag, " %s", version_get_githash(NULL));
        // Make uppercase
        for(size_t i = 0; i < furi_string_size(app->version_tag); ++i) {
            furi_string_set_char(
                app->version_tag, i, toupper(furi_string_get_char(app->version_tag, i)));
        }
        // Remove sha digits if necessary
        while(canvas_string_width(canvas, furi_string_get_cstr(app->version_tag)) >=
              canvas_width(canvas) - 8) {
            furi_string_left(app->version_tag, furi_string_size(app->version_tag) - 1);
        }
    } else {
        // Make uppercase, add space, add build date
        furi_string_replace(app->version_tag, "mntm", "MNTM");
        furi_string_cat_printf(app->version_tag, " %s", version_get_builddate(NULL));
    }
    // Add spaces to align right
    while(canvas_string_width(canvas, furi_string_get_cstr(app->version_tag)) <=
          canvas_width(canvas) - 13) {
        furi_string_replace_at(app->version_tag, separator, 0, " ");
    }
    gui_direct_draw_release(app->gui);

    return app;
}

void momentum_app_free(MomentumApp* app) {
    furi_assert(app);

    // Gui modules
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewVarItemList);
    variable_item_list_free(app->var_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewByteInput);
    byte_input_free(app->byte_input);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewNumberInput);
    number_input_free(app->number_input);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, MomentumAppViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    // View Dispatcher and Scene Manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Settings deinit

    CharList_it_t it;
    for(CharList_it(it, app->asset_pack_names); !CharList_end_p(it); CharList_next(it)) {
        free(*CharList_cref(it));
    }
    CharList_clear(app->asset_pack_names);

    momentum_app_empty_mainmenu_apps(app);
    CharList_clear(app->mainmenu_app_labels);
    CharList_clear(app->mainmenu_app_exes);

    FrequencyList_clear(app->subghz_static_freqs);
    FrequencyList_clear(app->subghz_hopper_freqs);

    furi_string_free(app->version_tag);

    // Records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_EXPANSION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_DOLPHIN);
    furi_record_close(RECORD_DESKTOP);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

extern int32_t momentum_app(void* p) {
    UNUSED(p);
    MomentumApp* app = momentum_app_alloc();
    scene_manager_next_scene(app->scene_manager, MomentumAppSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    momentum_app_free(app);
    return 0;
}
