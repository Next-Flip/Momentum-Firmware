#include "../archive_i.h"
#include "../helpers/archive_browser.h"
#include <mbedtls/md5.h>

#define TAG "Archive"

const char* units[] = {"Bytes", "KiB", "MiB", "GiB", "TiB"};

void archive_scene_info_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

static uint32_t archive_scene_info_dirwalk(void* context) {
    furi_assert(context);
    ArchiveApp* instance = context;

    char buf[128];
    FileInfo fileinfo;
    uint64_t total = 0;
    DirWalk* dir_walk = dir_walk_alloc(furi_record_open(RECORD_STORAGE));
    ArchiveFile_t* current = archive_get_current_file(instance->browser);
    if(dir_walk_open(dir_walk, furi_string_get_cstr(current->path))) {
        while(scene_manager_get_scene_state(instance->scene_manager, ArchiveAppSceneInfo)) {
            DirWalkResult result = dir_walk_read(dir_walk, NULL, &fileinfo);
            if(result == DirWalkError) {
                widget_element_text_box_set_text(instance->element, "Size: \e#Error\e#");
                break;
            }
            bool is_last = result == DirWalkLast;
            if(!file_info_is_dir(&fileinfo) || is_last) {
                if(!is_last) total += fileinfo.size;
                double show = total;
                size_t unit;
                for(unit = 0; unit < COUNT_OF(units); unit++) {
                    if(show < 1024) break;
                    show /= 1024;
                }
                snprintf(
                    buf,
                    sizeof(buf),
                    unit ? "Size: %s\e#%.2f\e# %s" : "Size: %s\e#%.0f\e# %s",
                    is_last ? "" : "... ",
                    show,
                    units[unit]);
                widget_element_text_box_set_text(instance->element, buf);
            }
            if(is_last) break;
        }
    } else {
        widget_element_text_box_set_text(instance->element, "Size: \e#Error\e#");
    }
    dir_walk_free(dir_walk);
    furi_record_close(RECORD_STORAGE);

    view_dispatcher_switch_to_view(instance->view_dispatcher, ArchiveViewWidget);
    return 0;
}

static uint32_t archive_scene_info_md5sum(void* context) {
    furi_assert(context);
    ArchiveApp* instance = context;

    // Based on lib/toolbox/md5_calc.c
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    ArchiveFile_t* current = archive_get_current_file(instance->browser);
    bool result = false;
    if(storage_file_open(
           file, furi_string_get_cstr(current->path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t output[16];
        const size_t size_to_read = 512;
        uint8_t* data = malloc(size_to_read);
        mbedtls_md5_context* md5_ctx = malloc(sizeof(mbedtls_md5_context));
        mbedtls_md5_init(md5_ctx);
        mbedtls_md5_starts(md5_ctx);
        while(scene_manager_get_scene_state(instance->scene_manager, ArchiveAppSceneInfo)) {
            size_t read_size = storage_file_read(file, data, size_to_read);
            if(storage_file_get_error(file) != FSE_OK) {
                break;
            }
            if(read_size == 0) {
                result = true;
                break;
            }
            mbedtls_md5_update(md5_ctx, data, read_size);
        }
        mbedtls_md5_finish(md5_ctx, output);
        if(result) {
            FuriString* md5 = furi_string_alloc_set("MD5: \e*");
            for(size_t i = 0; i < sizeof(output); i++) {
                furi_string_cat_printf(md5, "%02x", output[i]);
            }
            furi_string_cat(md5, "\e*");
            widget_element_text_box_set_text(instance->element, furi_string_get_cstr(md5));
        }
        free(md5_ctx);
        free(data);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!result) {
        char buf[64];
        strlcpy(buf, "MD5: \e*Error", sizeof(buf));
        uint8_t padding = 32 - strlen("Error");
        for(uint8_t i = 0; i < padding; i++) {
            strlcat(buf, " ", sizeof(buf));
        }
        strlcat(buf, "\e*", sizeof(buf));
        widget_element_text_box_set_text(instance->element, buf);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, ArchiveViewWidget);
    return 0;
}

void archive_scene_info_on_enter(void* context) {
    furi_assert(context);
    ArchiveApp* instance = context;

    widget_add_button_element(
        instance->widget, GuiButtonTypeLeft, "Back", archive_scene_info_widget_callback, instance);

    FuriString* filename = furi_string_alloc();
    FuriString* dirname = furi_string_alloc();

    ArchiveFile_t* current = archive_get_current_file(instance->browser);
    char buf[128];

    // Filename
    path_extract_filename(current->path, filename, false);
    snprintf(buf, sizeof(buf), "\e#%s\e#", furi_string_get_cstr(filename));
    widget_add_text_box_element(instance->widget, 1, 1, 126, 13, AlignLeft, AlignTop, buf, true);

    // Directory path
    path_extract_dirname(furi_string_get_cstr(current->path), dirname);
    widget_add_text_box_element(
        instance->widget, 1, 12, 126, 20, AlignLeft, AlignTop, furi_string_get_cstr(dirname), true);

    // This one to return and cursor select this file
    path_extract_filename_no_ext(furi_string_get_cstr(current->path), filename);
    strlcpy(instance->text_store, furi_string_get_cstr(filename), MAX_NAME_LEN);

    furi_string_free(filename);
    furi_string_free(dirname);

    // File size
    FileInfo fileinfo;
    bool is_dir = false;
    if(storage_common_stat(
           furi_record_open(RECORD_STORAGE), furi_string_get_cstr(current->path), &fileinfo) !=
       FSE_OK) {
        snprintf(buf, sizeof(buf), "Size: \e#Error\e#");
    } else if(file_info_is_dir(&fileinfo)) {
        is_dir = true;
        snprintf(buf, sizeof(buf), "Size: ... \e#0\e# %s", units[0]);

    } else {
        double show = fileinfo.size;
        size_t unit;
        for(unit = 0; unit < COUNT_OF(units); unit++) {
            if(show < 1024) break;
            show /= 1024;
        }
        snprintf(
            buf,
            sizeof(buf),
            unit ? "Size: \e#%.2f\e# %s" : "Size: \e#%.0f\e# %s",
            show,
            units[unit]);
    }
    WidgetElement* element = widget_add_text_box_element(
        instance->widget, 1, 31, 126, 13, AlignLeft, AlignTop, buf, true);

    // MD5 hash
    if(!is_dir) {
        strlcpy(buf, "MD5: \e*Loading...", sizeof(buf));
        uint8_t padding = 32 - strlen("Loading...");
        for(uint8_t i = 0; i < padding; i++) {
            strlcat(buf, " ", sizeof(buf));
        }
        strlcat(buf, "\e*", sizeof(buf));
        element = widget_add_text_box_element(
            instance->widget, 0, 43, 128, 24, AlignRight, AlignTop, buf, false);
    }

    instance->element = element;
    furi_record_close(RECORD_STORAGE);

    view_dispatcher_switch_to_view(instance->view_dispatcher, ArchiveViewWidget);

    scene_manager_set_scene_state(instance->scene_manager, ArchiveAppSceneInfo, true);
    instance->thread = furi_thread_alloc_ex(
        "ArchiveInfoWorker",
        1024,
        (FuriThreadCallback)(is_dir ? archive_scene_info_dirwalk : archive_scene_info_md5sum),
        instance);
    furi_thread_start(instance->thread);
}

bool archive_scene_info_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void archive_scene_info_on_exit(void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;

    scene_manager_set_scene_state(app->scene_manager, ArchiveAppSceneInfo, false);
    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
    widget_reset(app->widget);
}
