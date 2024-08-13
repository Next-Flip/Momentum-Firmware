#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexBatteryIcon,
    VarItemListIndexShowClock,
    VarItemListIndexStatusIcons,
    VarItemListIndexBarBorders,
    VarItemListIndexBarBackground,
};

void momentum_app_scene_interface_statusbar_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

const char* const battery_icon_names[BatteryIconCount] = {
    "OFF",
    "Bar",
    "%",
    "Inv. %",
    "Retro 3",
    "Retro 5",
    "Bar %",
};
static void momentum_app_scene_interface_statusbar_battery_icon_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, battery_icon_names[index]);
    momentum_settings.battery_icon = index;
    app->save_settings = true;
}

static void momentum_app_scene_interface_statusbar_statusbar_clock_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    app->desktop_settings.display_clock = value;
    app->save_desktop = true;
}

static void momentum_app_scene_interface_statusbar_status_icons_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.status_icons = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_statusbar_bar_borders_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.bar_borders = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_statusbar_bar_background_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.bar_background = value;
    app->save_settings = true;
}

void momentum_app_scene_interface_statusbar_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list,
        "Battery Icon",
        BatteryIconCount,
        momentum_app_scene_interface_statusbar_battery_icon_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.battery_icon);
    variable_item_set_current_value_text(item, battery_icon_names[momentum_settings.battery_icon]);

    item = variable_item_list_add(
        var_item_list,
        "Show Clock",
        2,
        momentum_app_scene_interface_statusbar_statusbar_clock_changed,
        app);
    variable_item_set_current_value_index(item, app->desktop_settings.display_clock);
    variable_item_set_current_value_text(item, app->desktop_settings.display_clock ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Status Icons",
        2,
        momentum_app_scene_interface_statusbar_status_icons_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.status_icons);
    variable_item_set_current_value_text(item, momentum_settings.status_icons ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Bar Borders",
        2,
        momentum_app_scene_interface_statusbar_bar_borders_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.bar_borders);
    variable_item_set_current_value_text(item, momentum_settings.bar_borders ? "ON" : "OFF");

    item = variable_item_list_add(
        var_item_list,
        "Bar Background",
        2,
        momentum_app_scene_interface_statusbar_bar_background_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.bar_background);
    variable_item_set_current_value_text(item, momentum_settings.bar_background ? "ON" : "OFF");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_interface_statusbar_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneInterfaceStatusbar));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_interface_statusbar_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, MomentumAppSceneInterfaceStatusbar, event.event);
        consumed = true;
        switch(event.event) {
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_interface_statusbar_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
