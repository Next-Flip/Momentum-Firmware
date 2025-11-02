#include "../infrared_app_i.h"

typedef enum {
    SubmenuIndexEditMetadataBrand,
    SubmenuIndexEditMetadataDeviceType,
    SubmenuIndexEditMetadataModel,
    SubmenuIndexEditMetadataContributor,
    SubmenuIndexEditMetadataRemoteModel,
} SubmenuIndexMetadata;

static void infrared_scene_edit_metadata_submenu_callback(void* context, uint32_t index) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, index);
}

void infrared_scene_edit_metadata_on_enter(void* context) {
    InfraredApp* infrared = context;
    Submenu* submenu = infrared->submenu;

    submenu_add_item(
        submenu,
        "Edit Brand",
        SubmenuIndexEditMetadataBrand,
        infrared_scene_edit_metadata_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Edit Device Type",
        SubmenuIndexEditMetadataDeviceType,
        infrared_scene_edit_metadata_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Edit Model",
        SubmenuIndexEditMetadataModel,
        infrared_scene_edit_metadata_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Edit Contributor",
        SubmenuIndexEditMetadataContributor,
        infrared_scene_edit_metadata_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Edit Remote Model",
        SubmenuIndexEditMetadataRemoteModel,
        infrared_scene_edit_metadata_submenu_callback,
        infrared);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(infrared->scene_manager, InfraredSceneEditMetadata));
    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewSubmenu);
}

bool infrared_scene_edit_metadata_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexEditMetadataBrand) {
            infrared->app_state.edit_target = InfraredEditTargetMetadataBrand;
            infrared->app_state.edit_mode = InfraredEditModeRename;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditRename);
            consumed = true;
        } else if(event.event == SubmenuIndexEditMetadataDeviceType) {
            infrared->app_state.edit_target = InfraredEditTargetMetadataDeviceType;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditSelectDeviceType);
            consumed = true;
        } else if(event.event == SubmenuIndexEditMetadataModel) {
            infrared->app_state.edit_target = InfraredEditTargetMetadataModel;
            infrared->app_state.edit_mode = InfraredEditModeRename;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditRename);
            consumed = true;
        } else if(event.event == SubmenuIndexEditMetadataContributor) {
            infrared->app_state.edit_target = InfraredEditTargetMetadataContributor;
            infrared->app_state.edit_mode = InfraredEditModeRename;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditRename);
            consumed = true;
        } else if(event.event == SubmenuIndexEditMetadataRemoteModel) {
            infrared->app_state.edit_target = InfraredEditTargetMetadataRemoteModel;
            infrared->app_state.edit_mode = InfraredEditModeRename;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditRename);
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_edit_metadata_on_exit(void* context) {
    InfraredApp* infrared = context;
    submenu_reset(infrared->submenu);
}
