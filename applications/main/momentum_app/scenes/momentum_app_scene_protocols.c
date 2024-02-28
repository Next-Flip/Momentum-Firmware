#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexBadkbMode,
    VarItemListIndexBadbtRemember,
    VarItemListIndexSubghzFreqs,
    VarItemListIndexSubghzExtend,
    VarItemListIndexGpioPins,
};

void momentum_app_scene_protocols_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void momentum_app_scene_protocols_bad_bt_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "BT" : "USB");
    momentum_settings.bad_bt = value;
    app->save_settings = true;
}

static void momentum_app_scene_protocols_bad_bt_remember_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.bad_bt_remember = value;
    app->save_settings = true;
}

static void momentum_app_scene_protocols_subghz_extend_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    app->subghz_extend = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");
    app->save_subghz = true;
}

static void momentum_app_scene_protocols_file_naming_prefix_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "After" : "Before");
    momentum_settings.file_naming_prefix_after = value;
    app->save_settings = true;
}

void momentum_app_scene_protocols_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list, "BadKB Mode", 2, momentum_app_scene_protocols_bad_bt_changed, app);
    variable_item_set_current_value_index(item, momentum_settings.bad_bt);
    variable_item_set_current_value_text(item, momentum_settings.bad_bt ? "BT" : "USB");

    item = variable_item_list_add(
        var_item_list,
        "BadBT Remember",
        2,
        momentum_app_scene_protocols_bad_bt_remember_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.bad_bt_remember);
    variable_item_set_current_value_text(item, momentum_settings.bad_bt_remember ? "ON" : "OFF");

    item = variable_item_list_add(var_item_list, "SubGHz Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(
        var_item_list, "SubGHz Extend", 2, momentum_app_scene_protocols_subghz_extend_changed, app);
    variable_item_set_current_value_index(item, app->subghz_extend);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");

    item = variable_item_list_add(var_item_list, "GPIO Pins", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(
        var_item_list,
        "File Naming Prefix",
        2,
        momentum_app_scene_protocols_file_naming_prefix_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.file_naming_prefix_after);
    variable_item_set_current_value_text(
        item, momentum_settings.file_naming_prefix_after ? "After" : "Before");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_protocols_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneProtocols));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_protocols_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocols, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexSubghzFreqs:
            scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocolsFreqs, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsFreqs);
            break;
        case VarItemListIndexGpioPins:
            scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneProtocolsGpio, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsGpio);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_protocols_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
