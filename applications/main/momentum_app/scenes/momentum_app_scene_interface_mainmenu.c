#include "../momentum_app.h"

#define ONLY_MSG "Only in PS4,\nVertical and\nMNTM styles!"
#define CANT_MSG "Can't show in\nthe selected\nstyle!"

enum VarItemListIndex {
    VarItemListIndexMenuStyle,
    VarItemListIndexResetMenu,
    VarItemListIndexItem,
    VarItemListIndexAddItem,
    VarItemListIndexMoveItem,
    VarItemListIndexRemoveItem,
};

void momentum_app_scene_interface_mainmenu_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

const char* const menu_style_names[MenuStyleCount] = {
    "List",
    "Wii",
    "DSi",
    "PS4",
    "Vertical",
    "C64",
    "Compact",
    "MNTM",
    "CoverFlow",
};
static void momentum_app_scene_interface_mainmenu_menu_style_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, menu_style_names[index]);
    momentum_settings.menu_style = index;
    app->save_settings = true;

    // Quick and dirty work around to refresh the list to update the locked items,
    // and isn't noticeable at all.
    scene_manager_previous_scene(app->scene_manager);
    scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceMainmenu);
}

static void momentum_app_scene_interface_mainmenu_name_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.menu_name = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_mainmenu_level_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.menu_level = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_mainmenu_time_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.menu_time = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_mainmenu_battery_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.menu_battery = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_mainmenu_otg_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.menu_otg = value;
    app->save_settings = true;
}

static void momentum_app_scene_interface_mainmenu_app_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    app->mainmenu_app_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
    size_t count = CharList_size(app->mainmenu_app_labels);
    char label[20];
    snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
    variable_item_set_item_label(item, label);
}

static void momentum_app_scene_interface_mainmenu_move_app_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t idx = app->mainmenu_app_index;
    size_t size = CharList_size(app->mainmenu_app_labels);
    uint8_t dir = variable_item_get_current_value_index(item);
    if(size >= 2) {
        if(dir == 2 && idx != size - 1) {
            // Right
            CharList_swap_at(app->mainmenu_app_labels, idx, idx + 1);
            CharList_swap_at(app->mainmenu_app_exes, idx, idx + 1);
            app->mainmenu_app_index++;
        } else if(dir == 0 && idx != 0) {
            // Left
            CharList_swap_at(app->mainmenu_app_labels, idx, idx - 1);
            CharList_swap_at(app->mainmenu_app_exes, idx, idx - 1);
            app->mainmenu_app_index--;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, VarItemListIndexMoveItem);
    }
    variable_item_set_current_value_index(item, 1);
}

void momentum_app_scene_interface_mainmenu_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    MenuStyle style = momentum_settings.menu_style;

    item = variable_item_list_add(
        var_item_list,
        "Menu Style",
        MenuStyleCount,
        momentum_app_scene_interface_mainmenu_menu_style_changed,
        app);
    variable_item_set_current_value_text(item, menu_style_names[momentum_settings.menu_style]);
    variable_item_set_current_value_index(item, momentum_settings.menu_style);

    variable_item_list_add(var_item_list, "Reset Menu", 0, NULL, app);

    size_t count = CharList_size(app->mainmenu_app_labels);
    item = variable_item_list_add(
        var_item_list, "Item", count, momentum_app_scene_interface_mainmenu_app_changed, app);
    if(count) {
        app->mainmenu_app_index = CLAMP(app->mainmenu_app_index, count - 1, 0U);
        char label[21];
        snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
        variable_item_set_item_label(item, label);
        variable_item_set_current_value_text(
            item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
    } else {
        app->mainmenu_app_index = 0;
        variable_item_set_current_value_text(item, "None");
    }
    variable_item_set_current_value_index(item, app->mainmenu_app_index);

    variable_item_list_add(var_item_list, "Add Item", 0, NULL, app);

    item = variable_item_list_add(
        var_item_list, "Move Item", 3, momentum_app_scene_interface_mainmenu_move_app_changed, app);
    variable_item_set_current_value_text(item, "");
    variable_item_set_current_value_index(item, 1);
    variable_item_set_locked(item, count < 2, "Can't move\nwith less\nthan 2 apps!");

    variable_item_list_add(var_item_list, "Remove Item", 0, NULL, app);

    bool lock_all = style != MenuStylePs4 && style != MenuStyleVertical && style != MenuStyleMNTM;
    bool lock_vertical = style == MenuStyleVertical;
    bool lock_ps4_vertical = style == MenuStylePs4 || style == MenuStyleVertical;

    item = variable_item_list_add(
        var_item_list, "Show Name", 2, momentum_app_scene_interface_mainmenu_name_changed, app);
    variable_item_set_current_value_index(item, momentum_settings.menu_name);
    variable_item_set_current_value_text(item, momentum_settings.menu_name ? "ON" : "OFF");
    variable_item_set_locked(item, lock_all || lock_vertical, lock_vertical ? CANT_MSG : ONLY_MSG);

    item = variable_item_list_add(
        var_item_list, "Show Level", 2, momentum_app_scene_interface_mainmenu_level_changed, app);
    variable_item_set_current_value_index(item, momentum_settings.menu_level);
    variable_item_set_current_value_text(item, momentum_settings.menu_level ? "ON" : "OFF");
    variable_item_set_locked(item, lock_all || lock_vertical, lock_vertical ? CANT_MSG : ONLY_MSG);

    item = variable_item_list_add(
        var_item_list, "Show Time", 2, momentum_app_scene_interface_mainmenu_time_changed, app);
    variable_item_set_current_value_index(item, momentum_settings.menu_time);
    variable_item_set_current_value_text(item, momentum_settings.menu_time ? "ON" : "OFF");
    variable_item_set_locked(item, lock_all, ONLY_MSG);

    item = variable_item_list_add(
        var_item_list,
        "Show Battery",
        2,
        momentum_app_scene_interface_mainmenu_battery_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.menu_battery);
    variable_item_set_current_value_text(item, momentum_settings.menu_battery ? "ON" : "OFF");
    variable_item_set_locked(item, lock_all, ONLY_MSG);

    item = variable_item_list_add(
        var_item_list, "Show OTG (5v)", 2, momentum_app_scene_interface_mainmenu_otg_changed, app);
    variable_item_set_current_value_index(item, momentum_settings.menu_otg);
    variable_item_set_current_value_text(item, momentum_settings.menu_otg ? "ON" : "OFF");
    variable_item_set_locked(
        item, lock_all || lock_ps4_vertical, lock_ps4_vertical ? CANT_MSG : ONLY_MSG);

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_interface_mainmenu_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneInterfaceMainmenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_interface_mainmenu_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, MomentumAppSceneInterfaceMainmenu, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexMenuStyle:
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceMainmenuStyle);
            break;
        case VarItemListIndexResetMenu:
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceMainmenuReset);
            break;
        case VarItemListIndexRemoveItem:
            if(!CharList_size(app->mainmenu_app_labels)) break;
            if(!CharList_size(app->mainmenu_app_exes)) break;
            free(*CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
            free(*CharList_get(app->mainmenu_app_exes, app->mainmenu_app_index));
            CharList_remove_v(
                app->mainmenu_app_labels, app->mainmenu_app_index, app->mainmenu_app_index + 1);
            CharList_remove_v(
                app->mainmenu_app_exes, app->mainmenu_app_index, app->mainmenu_app_index + 1);
            /* fall through */
        case VarItemListIndexMoveItem: {
            app->save_mainmenu_apps = true;
            size_t count = CharList_size(app->mainmenu_app_labels);
            VariableItem* item = variable_item_list_get(app->var_item_list, VarItemListIndexItem);
            if(count) {
                app->mainmenu_app_index = CLAMP(app->mainmenu_app_index, count - 1, 0U);
                char label[21];
                snprintf(label, sizeof(label), "Item  %u/%u", 1 + app->mainmenu_app_index, count);
                variable_item_set_item_label(item, label);
                variable_item_set_current_value_text(
                    item, *CharList_get(app->mainmenu_app_labels, app->mainmenu_app_index));
            } else {
                app->mainmenu_app_index = 0;
                variable_item_set_item_label(item, "Item");
                variable_item_set_current_value_text(item, "None");
            }
            variable_item_set_current_value_index(item, app->mainmenu_app_index);
            variable_item_set_values_count(item, count);
            break;
        }
        case VarItemListIndexAddItem:
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceMainmenuAdd);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_interface_mainmenu_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
