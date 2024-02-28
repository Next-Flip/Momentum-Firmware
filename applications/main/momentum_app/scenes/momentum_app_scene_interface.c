#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexGraphics,
    VarItemListIndexMainmenu,
    VarItemListIndexLockscreen,
    VarItemListIndexStatusbar,
    VarItemListIndexFileBrowser,
};

void momentum_app_scene_interface_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void momentum_app_scene_interface_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "Graphics", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Mainmenu", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Lockscreen", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Statusbar", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "File Browser", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_interface_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneInterface));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_interface_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneInterface, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexGraphics:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneInterfaceGraphics, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceGraphics);
            break;
        case VarItemListIndexMainmenu:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneInterfaceMainmenu, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceMainmenu);
            break;
        case VarItemListIndexLockscreen:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneInterfaceLockscreen, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceLockscreen);
            break;
        case VarItemListIndexStatusbar:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneInterfaceStatusbar, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceStatusbar);
            break;
        case VarItemListIndexFileBrowser:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneInterfaceFilebrowser, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceFilebrowser);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_interface_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
