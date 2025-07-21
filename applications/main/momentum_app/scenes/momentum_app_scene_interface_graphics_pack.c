#include "../momentum_app.h"

void momentum_app_scene_interface_graphics_pack_submenu_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void momentum_app_scene_interface_graphics_pack_on_enter(void* context) {
    MomentumApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu, "Default", 0, momentum_app_scene_interface_graphics_pack_submenu_callback, app);

    for(size_t i = 0; i < CharList_size(app->asset_pack_names); i++) {
        submenu_add_item(
            submenu,
            *CharList_get(app->asset_pack_names, i),
            i + 1,
            momentum_app_scene_interface_graphics_pack_submenu_callback,
            app);
    }

    submenu_set_header(submenu, "Choose Asset Pack:");

    submenu_set_selected_item(submenu, app->asset_pack_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewSubmenu);
}

bool momentum_app_scene_interface_graphics_pack_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        strlcpy(
            momentum_settings.asset_pack,
            event.event == 0 ? "" : *CharList_get(app->asset_pack_names, event.event - 1),
            ASSET_PACKS_NAME_LEN);
        app->asset_pack_index = event.event;
        app->save_settings = true;
        app->apply_pack = true;
        scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void momentum_app_scene_interface_graphics_pack_on_exit(void* context) {
    MomentumApp* app = context;
    submenu_reset(app->submenu);
}
