#include <gui/gui.h>
#include <gui/view_holder.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <assets_icons.h>
#include <applications.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_menuapp.h"

#define TAG "LoaderMenu"

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
    bool settings_only;
    bool in_settings;
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
            loader_menu->thread = furi_thread_alloc_ex(TAG, 1024, loader_menu_thread, loader_menu);
            furi_thread_start(loader_menu->thread);
        }
    }
}

static void loader_menu_set_view(LoaderMenu* loader_menu, View* view) {
    view_holder_set_view(loader_menu->view_holder, view);
    if(view) {
        view_holder_update(view, loader_menu->view_holder);
    }
}

static void loader_menu_dummy_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    uint8_t x = canvas_width(canvas) / 2 - 24 / 2;
    uint8_t y = canvas_height(canvas) / 2 - 24 / 2;

    canvas_draw_icon(canvas, x, y, &I_LoadingHourglass_24x24);
}

enum {
    LoaderMenuIndexApps = (uint32_t)-1,
    LoaderMenuIndexLast = (uint32_t)-2,
    LoaderMenuIndexSettings = (uint32_t)-3,
};

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context, bool settings_only) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->selected_primary = LoaderMenuIndexApps;
    loader_menu->selected_setting = 0;
    loader_menu->settings_only = settings_only;
    loader_menu->in_settings = settings_only;

    loader_menu->dummy = view_alloc();
    view_set_draw_callback(loader_menu->dummy, loader_menu_dummy_draw);

    Gui* gui = furi_record_open(RECORD_GUI);
    loader_menu->view_holder = view_holder_alloc();
    view_holder_attach_to_gui(loader_menu->view_holder, gui);
    view_holder_set_back_callback(loader_menu->view_holder, NULL, NULL);
    loader_menu_set_view(loader_menu, loader_menu->dummy);
    view_holder_start(loader_menu->view_holder);

    loader_menu->loader = furi_record_open(RECORD_LOADER);
    loader_menu->subscription = furi_pubsub_subscribe(
        loader_get_pubsub(loader_menu->loader), loader_pubsub_callback, loader_menu);

    loader_menu->thread = furi_thread_alloc_ex(TAG, 1024, loader_menu_thread, loader_menu);
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

    view_holder_free(loader_menu->view_holder);
    furi_record_close(RECORD_GUI);

    view_free(loader_menu->dummy);

    free(loader_menu);
}

typedef struct {
    LoaderMenu* loader_menu;
    Menu* primary_menu;
    Submenu* settings_menu;
} LoaderMenuApp;

static void loader_menu_primary_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    const char* launch = NULL;
    switch(index) {
    case LoaderMenuIndexApps:
        launch = LOADER_APPLICATIONS_NAME;
        break;
    case LoaderMenuIndexLast:
        launch = FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1].name;
        break;
    case LoaderMenuIndexSettings:
        loader_menu_set_view(app->loader_menu, submenu_get_view(app->settings_menu));
        app->loader_menu->in_settings = true;
        break;
    default:
        MenuAppList_t* menu_apps = loader_get_menu_apps(app->loader_menu->loader);
        if(index < MenuAppList_size(*menu_apps)) {
            launch = MenuAppList_get(*menu_apps, index)->exe;
        }
        break;
    }
    if(launch) {
        loader_start_detached_with_gui_error(app->loader_menu->loader, launch, NULL);
    }
}

static void loader_menu_settings_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    if(index < FLIPPER_SETTINGS_APPS_COUNT) {
        const char* launch = FLIPPER_SETTINGS_APPS[index].name;
        loader_start_detached_with_gui_error(app->loader_menu->loader, launch, NULL);
    }
}

static void loader_menu_back(void* context) {
    LoaderMenuApp* app = context;
    if(app->loader_menu->in_settings && !app->loader_menu->settings_only) {
        loader_menu_set_view(app->loader_menu, menu_get_view(app->primary_menu));
        app->loader_menu->in_settings = false;
    } else {
        furi_thread_flags_set(furi_thread_get_id(app->loader_menu->thread), 0);
        if(app->loader_menu->closed_cb) {
            app->loader_menu->closed_cb(app->loader_menu->context);
        }
    }
}

static void loader_menu_build_menu(LoaderMenuApp* app, LoaderMenu* menu) {
    menu_add_item(
        app->primary_menu,
        LOADER_APPLICATIONS_NAME,
        &A_Plugins_14,
        LoaderMenuIndexApps,
        loader_menu_primary_callback,
        app);

    MenuAppList_t* menu_apps = loader_get_menu_apps(menu->loader);
    for(size_t i = 0; i < MenuAppList_size(*menu_apps); i++) {
        const MenuApp* menu_app = MenuAppList_get(*menu_apps, i);
        menu_add_item(
            app->primary_menu,
            menu_app->label,
            menu_app->icon,
            i,
            loader_menu_primary_callback,
            app);
    }

    const FlipperExternalApplication* last =
        &FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1];
    menu_add_item(
        app->primary_menu,
        last->name,
        last->icon,
        LoaderMenuIndexLast,
        loader_menu_primary_callback,
        app);
    menu_add_item(
        app->primary_menu,
        "Settings",
        &A_Settings_14,
        LoaderMenuIndexSettings,
        loader_menu_primary_callback,
        app);

    menu_set_selected_item(app->primary_menu, menu->selected_primary);
};

static void loader_menu_build_submenu(LoaderMenuApp* app, LoaderMenu* loader_menu) {
    for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
        submenu_add_item(
            app->settings_menu,
            FLIPPER_SETTINGS_APPS[i].name,
            i,
            loader_menu_settings_callback,
            app);
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

    View* view = app->loader_menu->in_settings ? submenu_get_view(app->settings_menu) :
                                                 menu_get_view(app->primary_menu);
    loader_menu_set_view(app->loader_menu, view);
    view_holder_set_back_callback(app->loader_menu->view_holder, loader_menu_back, app);

    return app;
}

static void loader_menu_app_free(LoaderMenuApp* app) {
    view_holder_set_back_callback(app->loader_menu->view_holder, NULL, NULL);
    loader_menu_set_view(app->loader_menu, app->loader_menu->dummy);

    if(!app->loader_menu->settings_only) {
        app->loader_menu->selected_primary = menu_get_selected_item(app->primary_menu);
        menu_free(app->primary_menu);
    }
    app->loader_menu->selected_setting =
        app->loader_menu->in_settings ? submenu_get_selected_item(app->settings_menu) : 0;
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
