#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexColors,
    VarItemListIndexForeground,
    VarItemListIndexBackground,
};

void momentum_app_scene_misc_vgm_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

const char* const colors_names[VgmColorModeCount] = {
    "Default",
    "Custom",
    "RGB Backlight",
};
static void momentum_app_scene_misc_vgm_colors_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, colors_names[index]);
    momentum_settings.vgm_color_mode = index;
    app->save_settings = true;
    variable_item_set_locked(
        variable_item_list_get(app->var_item_list, VarItemListIndexForeground),
        index != VgmColorModeCustom,
        NULL);
    variable_item_set_locked(
        variable_item_list_get(app->var_item_list, VarItemListIndexBackground),
        index != VgmColorModeCustom,
        NULL);
    expansion_disable(app->expansion);
    expansion_enable(app->expansion);
}

static const struct {
    char* name;
    Rgb565Color color;
} vgm_colors[] = {
    {"Orange", {0xFC00}},
    {"Black", {0x0000}},
};
static const size_t vgm_colors_count = COUNT_OF(vgm_colors);
static void momentum_app_scene_misc_vgm_foreground_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, vgm_colors[index].name);
    momentum_settings.vgm_color_fg = vgm_colors[index].color;
    app->save_settings = true;
    if(momentum_settings.vgm_color_mode == VgmColorModeCustom) {
        expansion_disable(app->expansion);
        expansion_enable(app->expansion);
    }
}
static void momentum_app_scene_misc_vgm_background_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, vgm_colors[index].name);
    momentum_settings.vgm_color_bg = vgm_colors[index].color;
    app->save_settings = true;
    if(momentum_settings.vgm_color_mode == VgmColorModeCustom) {
        expansion_disable(app->expansion);
        expansion_enable(app->expansion);
    }
}

void momentum_app_scene_misc_vgm_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        var_item_list,
        "VGM Colors",
        VgmColorModeCount,
        momentum_app_scene_misc_vgm_colors_changed,
        app);
    value_index = momentum_settings.vgm_color_mode;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, colors_names[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Foreground",
        vgm_colors_count,
        momentum_app_scene_misc_vgm_foreground_changed,
        app);
    Rgb565Color color = momentum_settings.vgm_color_fg;
    bool found = false;
    for(size_t i = 0; i < vgm_colors_count; i++) {
        if(rgb565cmp(&color, &vgm_colors[i].color) != 0) continue;
        value_index = i;
        found = true;
        break;
    }
    variable_item_set_current_value_index(item, found ? value_index : vgm_colors_count);
    if(found) {
        variable_item_set_current_value_text(item, vgm_colors[value_index].name);
    } else {
        char str[5];
        snprintf(str, sizeof(str), "%04X", color.value);
        variable_item_set_current_value_text(item, str);
    }
    variable_item_set_locked(
        item, momentum_settings.vgm_color_mode != VgmColorModeCustom, "Need Custom\nColors!");

    item = variable_item_list_add(
        var_item_list,
        "Background",
        vgm_colors_count,
        momentum_app_scene_misc_vgm_background_changed,
        app);
    color = momentum_settings.vgm_color_bg;
    found = false;
    for(size_t i = 0; i < vgm_colors_count; i++) {
        if(rgb565cmp(&color, &vgm_colors[i].color) != 0) continue;
        value_index = i;
        found = true;
        break;
    }
    variable_item_set_current_value_index(item, found ? value_index : vgm_colors_count);
    if(found) {
        variable_item_set_current_value_text(item, vgm_colors[value_index].name);
    } else {
        char str[5];
        snprintf(str, sizeof(str), "%04X", color.value);
        variable_item_set_current_value_text(item, str);
    }
    variable_item_set_locked(
        item, momentum_settings.vgm_color_mode != VgmColorModeCustom, "Need Custom\nColors!");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_misc_vgm_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneMiscVgm));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_misc_vgm_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneMiscVgm, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexForeground:
        case VarItemListIndexBackground:
            scene_manager_set_scene_state(
                app->scene_manager,
                MomentumAppSceneMiscVgmColor,
                event.event - VarItemListIndexForeground);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneMiscVgmColor);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_misc_vgm_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
