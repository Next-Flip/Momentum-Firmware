#include <gui/gui.h>
#include <gui/view_holder.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <assets_icons.h>
#include <applications.h>
#include <toolbox/run_parallel.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_menu_storage_i.h"

#include <flipper_application/flipper_application.h>
#include <toolbox/stream/file_stream.h>
#include <gui/modules/file_browser.h>
#include <core/dangerous_defines.h>
#include <momentum/momentum.h>
#include <gui/icon_i.h>
#include <m-list.h>

#define TAG "LoaderMenu"

typedef enum {
    LoaderMenuViewPrimary,
    LoaderMenuViewSettings,
} LoaderMenuView;

struct LoaderMenu {
    FuriThread* thread;
    void (*closed_cb)(void*);
    void* context;

    View* dummy;
    ViewHolder* view_holder;

    Loader* loader;
    FuriPubSubSubscription* subscription;

    uint32_t selected_primary;
    uint32_t selected_setting;
    LoaderMenuView current_view;
    bool settings_only;
};

static int32_t loader_menu_thread(void* p);

static void loader_pubsub_callback(const void* message, void* context) {
    const LoaderEvent* event = message;
    LoaderMenu* loader_menu = context;

    if(event->type == LoaderEventTypeApplicationBeforeLoad) {
        if(loader_menu->thread) {
            furi_thread_flags_set(furi_thread_get_id(loader_menu->thread), 0);
            furi_thread_join(loader_menu->thread);
            furi_thread_free(loader_menu->thread);
            loader_menu->thread = NULL;
        }
    } else if(
        event->type == LoaderEventTypeApplicationLoadFailed ||
        event->type == LoaderEventTypeApplicationStopped) {
        if(!loader_menu->thread) {
            loader_menu->thread = furi_thread_alloc_ex(TAG, 2048, loader_menu_thread, loader_menu);
            furi_thread_start(loader_menu->thread);
        }
    }
}

static void loader_menu_dummy_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    uint8_t x = canvas_width(canvas) / 2 - 24 / 2;
    uint8_t y = canvas_height(canvas) / 2 - 24 / 2;

    canvas_draw_icon(canvas, x, y, &I_LoadingHourglass_24x24);
}

enum {
    LoaderMenuIndexApplications = (uint32_t)-1,
    LoaderMenuIndexLast = (uint32_t)-2,
    LoaderMenuIndexSettings = (uint32_t)-3,
};

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context, bool settings_only) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->selected_primary = LoaderMenuIndexApplications;
    loader_menu->selected_setting = 0;
    loader_menu->settings_only = settings_only;
    loader_menu->current_view = settings_only ? LoaderMenuViewSettings : LoaderMenuViewPrimary;

    loader_menu->dummy = view_alloc();
    view_set_draw_callback(loader_menu->dummy, loader_menu_dummy_draw);

    Gui* gui = furi_record_open(RECORD_GUI);
    loader_menu->view_holder = view_holder_alloc();
    view_holder_attach_to_gui(loader_menu->view_holder, gui);
    view_holder_set_back_callback(loader_menu->view_holder, NULL, NULL);
    view_holder_set_view(loader_menu->view_holder, loader_menu->dummy);

    loader_menu->loader = furi_record_open(RECORD_LOADER);
    loader_menu->subscription = furi_pubsub_subscribe(
        loader_get_pubsub(loader_menu->loader), loader_pubsub_callback, loader_menu);

    loader_menu->thread = furi_thread_alloc_ex(TAG, 2048, loader_menu_thread, loader_menu);
    furi_thread_start(loader_menu->thread);
    return loader_menu;
}

void loader_menu_free(LoaderMenu* loader_menu) {
    furi_assert(loader_menu);

    furi_pubsub_unsubscribe(loader_get_pubsub(loader_menu->loader), loader_menu->subscription);
    furi_record_close(RECORD_LOADER);

    if(loader_menu->thread) {
        furi_thread_join(loader_menu->thread);
        furi_thread_free(loader_menu->thread);
    }

    view_holder_set_view(loader_menu->view_holder, NULL);
    view_holder_free(loader_menu->view_holder);
    furi_record_close(RECORD_GUI);

    view_free(loader_menu->dummy);

    free(loader_menu);
}

typedef struct {
    const char* name;
    const Icon* icon;
    const char* path;
} MenuApp;

LIST_DEF(MenuAppList, MenuApp, M_POD_OPLIST)
#define M_OPL_MenuAppList_t() LIST_OPLIST(MenuAppList)

typedef struct {
    LoaderMenu* loader_menu;
    Menu* primary_menu;
    Submenu* settings_menu;
    MenuAppList_t apps_list;
} LoaderMenuApp;

static void loader_menu_start(const char* name) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_detached_with_gui_error(loader, name, NULL);
    furi_record_close(RECORD_LOADER);
}

static void loader_menu_apps_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    const MenuApp* menu_app = MenuAppList_get(app->apps_list, index);
    const char* name = menu_app->path ? menu_app->path : menu_app->name;
    loader_menu_start(name);
}

static void loader_menu_last_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    const char* path = FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1].name;
    loader_menu_start(path);
}

static void loader_menu_applications_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    const char* name = LOADER_APPLICATIONS_NAME;
    loader_menu_start(name);
}

static void loader_menu_settings_menu_callback(void* context, uint32_t index) {
    UNUSED(context);
    const char* name = FLIPPER_SETTINGS_APPS[index].name;

    // Workaround for SD format when app can't be opened
    if(!strcmp(name, "Storage")) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FS_Error status = storage_sd_status(storage);
        furi_record_close(RECORD_STORAGE);
        // If SD card not ready, cannot be formatted, so we want loader to give
        // normal error message, with function below
        if(status != FSE_NOT_READY) {
            // Attempt to launch the app, and if failed offer to format SD card
            run_parallel(loader_menu_storage_settings, storage, 512);
            return;
        }
    }

    loader_menu_start(name);
}

// Can't do this in GUI callbacks because now ViewHolder waits for ongoing
// input, and inputs are not processed because GUI is processing callbacks
static void loader_menu_set_view_pending(void* context, uint32_t arg) {
    LoaderMenuApp* app = context;
    view_holder_set_view(app->loader_menu->view_holder, (View*)arg);
}

static void loader_menu_switch_to_settings(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    furi_timer_pending_callback(
        loader_menu_set_view_pending, app, (uint32_t)submenu_get_view(app->settings_menu));
    app->loader_menu->current_view = LoaderMenuViewSettings;
}

static void loader_menu_back(void* context) {
    LoaderMenuApp* app = context;
    if(app->loader_menu->current_view == LoaderMenuViewSettings &&
       !app->loader_menu->settings_only) {
        furi_timer_pending_callback(
            loader_menu_set_view_pending, app, (uint32_t)menu_get_view(app->primary_menu));
        app->loader_menu->current_view = LoaderMenuViewPrimary;
    } else {
        furi_thread_flags_set(furi_thread_get_id(app->loader_menu->thread), 0);
        if(app->loader_menu->closed_cb) {
            app->loader_menu->closed_cb(app->loader_menu->context);
        }
    }
}

static void loader_menu_add_app_entry(
    LoaderMenuApp* app,
    const char* name,
    const Icon* icon,
    const char* path) {
    MenuAppList_push_back(app->apps_list, (MenuApp){name, icon, path});
    menu_add_item(
        app->primary_menu,
        name,
        icon,
        MenuAppList_size(app->apps_list) - 1,
        loader_menu_apps_callback,
        app);
}

bool loader_menu_load_fap_meta(
    Storage* storage,
    FuriString* path,
    FuriString* name,
    const Icon** icon) {
    *icon = NULL;
    uint8_t* icon_buf = malloc(CUSTOM_ICON_MAX_SIZE);
    if(!flipper_application_load_name_and_icon(path, storage, &icon_buf, name)) {
        free(icon_buf);
        icon_buf = NULL;
        return false;
    }
    *icon = malloc(sizeof(Icon));
    FURI_CONST_ASSIGN((*icon)->frame_count, 1);
    FURI_CONST_ASSIGN((*icon)->frame_rate, 1);
    FURI_CONST_ASSIGN((*icon)->width, 10);
    FURI_CONST_ASSIGN((*icon)->height, 10);
    FURI_CONST_ASSIGN_PTR((*icon)->frames, malloc(sizeof(const uint8_t*)));
    FURI_CONST_ASSIGN_PTR((*icon)->frames[0], icon_buf);
    return true;
}

static void loader_menu_find_add_app(LoaderMenuApp* app, Storage* storage, FuriString* line) {
    const char* name = NULL;
    const Icon* icon = NULL;
    const char* path = NULL;
    if(furi_string_start_with(line, "/")) {
        path = strdup(furi_string_get_cstr(line));
        if(!loader_menu_load_fap_meta(storage, line, line, &icon)) {
            free((void*)path);
            path = NULL;
        } else {
            name = strdup(furi_string_get_cstr(line));
        }
    } else {
        for(size_t i = 0; !name && i < FLIPPER_APPS_COUNT; i++) {
            if(furi_string_equal(line, FLIPPER_APPS[i].name)) {
                name = FLIPPER_APPS[i].name;
                icon = FLIPPER_APPS[i].icon;
            }
        }
        for(size_t i = 0; !name && i < FLIPPER_EXTERNAL_APPS_COUNT; i++) {
            if(furi_string_equal(line, FLIPPER_EXTERNAL_APPS[i].name)) {
                name = FLIPPER_EXTERNAL_APPS[i].name;
                icon = FLIPPER_EXTERNAL_APPS[i].icon;
            }
        }
    }
    // Path only set for FAPs
    if(name && icon) {
        loader_menu_add_app_entry(app, name, icon, path);
    }
}

static void loader_menu_build_menu(LoaderMenuApp* app, LoaderMenu* menu) {
    menu_add_item(
        app->primary_menu,
        LOADER_APPLICATIONS_NAME,
        &A_Plugins_14,
        LoaderMenuIndexApplications,
        loader_menu_applications_callback,
        NULL);

    MenuAppList_init(app->apps_list);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    uint32_t version;
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
            loader_menu_find_add_app(app, storage, line);
        }
    } else {
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            loader_menu_add_app_entry(app, FLIPPER_APPS[i].name, FLIPPER_APPS[i].icon, NULL);
        }
        // Until count - 1 because last app is hardcoded below
        for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT - 1; i++) {
            loader_menu_add_app_entry(
                app, FLIPPER_EXTERNAL_APPS[i].name, FLIPPER_EXTERNAL_APPS[i].icon, NULL);
        }
    }
    furi_string_free(line);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    const FlipperExternalApplication* last =
        &FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1];
    menu_add_item(
        app->primary_menu,
        last->name,
        last->icon,
        LoaderMenuIndexLast,
        loader_menu_last_callback,
        NULL);
    menu_add_item(
        app->primary_menu,
        "Settings",
        &A_Settings_14,
        LoaderMenuIndexSettings,
        loader_menu_switch_to_settings,
        app);

    menu_set_selected_item(app->primary_menu, menu->selected_primary);
}

static void loader_menu_build_submenu(LoaderMenuApp* app, LoaderMenu* loader_menu) {
    for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
        submenu_add_item(
            app->settings_menu,
            FLIPPER_SETTINGS_APPS[i].name,
            i,
            loader_menu_settings_menu_callback,
            NULL);
    }
    submenu_set_selected_item(app->settings_menu, loader_menu->selected_setting);
}

static LoaderMenuApp* loader_menu_app_alloc(LoaderMenu* loader_menu) {
    LoaderMenuApp* app = malloc(sizeof(LoaderMenuApp));
    app->loader_menu = loader_menu;

    // Primary menu
    if(!app->loader_menu->settings_only) {
        app->primary_menu = menu_alloc();
        loader_menu_build_menu(app, loader_menu);
    }

    // Settings menu
    app->settings_menu = submenu_alloc();
    loader_menu_build_submenu(app, loader_menu);

    View* view = app->loader_menu->current_view == LoaderMenuViewSettings ?
                     submenu_get_view(app->settings_menu) :
                     menu_get_view(app->primary_menu);
    view_holder_set_view(app->loader_menu->view_holder, view);
    view_holder_set_back_callback(app->loader_menu->view_holder, loader_menu_back, app);

    return app;
}

static void loader_menu_app_free(LoaderMenuApp* app) {
    view_holder_set_back_callback(app->loader_menu->view_holder, NULL, NULL);
    view_holder_set_view(app->loader_menu->view_holder, app->loader_menu->dummy);

    if(!app->loader_menu->settings_only) {
        app->loader_menu->selected_primary = menu_get_selected_item(app->primary_menu);
        menu_free(app->primary_menu);
        for
            M_EACH(menu_app, app->apps_list, MenuAppList_t) {
                // Path only set for FAPs, if unset then name and
                // icon point to flash and must not be freed
                if(menu_app->path) {
                    free((void*)menu_app->name);
                    free((void*)menu_app->icon->frames[0]);
                    free((void*)menu_app->icon->frames);
                    free((void*)menu_app->icon);
                    free((void*)menu_app->path);
                }
            }
        MenuAppList_clear(app->apps_list);
    }
    app->loader_menu->selected_setting = app->loader_menu->current_view == LoaderMenuViewSettings ?
                                             submenu_get_selected_item(app->settings_menu) :
                                             0;
    submenu_free(app->settings_menu);

    free(app);
}

static int32_t loader_menu_thread(void* p) {
    LoaderMenu* loader_menu = p;
    furi_assert(loader_menu);

    LoaderMenuApp* app = loader_menu_app_alloc(loader_menu);

    furi_thread_flags_wait(0, FuriFlagWaitAll, FuriWaitForever);

    loader_menu_app_free(app);

    return 0;
}
