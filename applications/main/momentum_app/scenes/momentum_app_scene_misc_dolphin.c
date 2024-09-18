#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexDolphinLevel,
    VarItemListIndexDolphinXp,
    VarItemListIndexDolphinAngry,
    VarItemListIndexButthurtTimer,
};

void momentum_app_scene_misc_dolphin_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void momentum_app_scene_misc_dolphin_dolphin_level_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);

    uint8_t index = variable_item_get_current_value_index(item);
    uint32_t xp = index > 0 ? DOLPHIN_LEVELS[index - 1] : 0;
    app->dolphin_xp = xp + 1; // Prevent levelup animation

    uint8_t level = dolphin_get_level(app->dolphin_xp);
    char level_str[4];
    snprintf(level_str, sizeof(level_str), "%u", level);
    variable_item_set_current_value_text(item, level_str);
    app->save_xp = true;
    app->save_dolphin = true;

    item = variable_item_list_get(app->var_item_list, VarItemListIndexDolphinXp);
    variable_item_set_current_value_index(
        item,
        app->dolphin_xp == 0              ? 0 :
        app->dolphin_xp == DOLPHIN_MAX_XP ? 2 :
                                            1);
    char xp_str[6];
    snprintf(xp_str, sizeof(xp_str), "%lu", app->dolphin_xp);
    variable_item_set_current_value_text(item, xp_str);
}

static void momentum_app_scene_misc_dolphin_dolphin_xp_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);

    // uin8_t index too small for all levels, use 3 fake items to
    // show buttons and change values in callback
    uint8_t direction = variable_item_get_current_value_index(item);
    if(app->dolphin_xp == DOLPHIN_MAX_XP) direction = 0;
    if(app->dolphin_xp == 0) direction = 2;
    if(direction == 0) app->dolphin_xp--;
    if(direction == 2) app->dolphin_xp++;

    variable_item_set_current_value_index(
        item,
        app->dolphin_xp == 0              ? 0 :
        app->dolphin_xp == DOLPHIN_MAX_XP ? 2 :
                                            1);
    char xp_str[6];
    snprintf(xp_str, sizeof(xp_str), "%lu", app->dolphin_xp);
    variable_item_set_current_value_text(item, xp_str);
    app->save_xp = true;
    app->save_dolphin = true;

    uint8_t level = dolphin_get_level(app->dolphin_xp);
    char level_str[4];
    snprintf(level_str, sizeof(level_str), "%u", level);
    variable_item_set_current_value_text(
        variable_item_list_get(app->var_item_list, VarItemListIndexDolphinLevel), level_str);
}

static void momentum_app_scene_misc_dolphin_dolphin_angry_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    app->dolphin_angry = variable_item_get_current_value_index(item);
    char angry_str[4];
    snprintf(angry_str, sizeof(angry_str), "%lu", app->dolphin_angry);
    variable_item_set_current_value_text(item, angry_str);
    app->save_angry = true;
    app->save_dolphin = true;
}

const char* const butthurt_timer_names[] = {
    "OFF",
    "30 M",
    "1 H",
    "2 H",
    "4 H",
    "6 H",
    "8 H",
    "12 H",
    "24 H",
    "48 H",
};
const uint32_t butthurt_timer_values[COUNT_OF(butthurt_timer_names)] = {
    0,
    1800,
    3600,
    7200,
    14400,
    21600,
    28800,
    43200,
    86400,
    172800,
};
static void momentum_app_scene_misc_dolphin_butthurt_timer_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, butthurt_timer_names[index]);
    momentum_settings.butthurt_timer = butthurt_timer_values[index];
    app->save_settings = true;
    app->save_dolphin = true;
}

void momentum_app_scene_misc_dolphin_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;
    DolphinSettings settings;
    dolphin_get_settings(app->dolphin, &settings);

    uint8_t level = dolphin_get_level(app->dolphin_xp);
    char level_str[4];
    snprintf(level_str, sizeof(level_str), "%u", level);
    item = variable_item_list_add(
        var_item_list,
        "Dolphin Level",
        DOLPHIN_LEVEL_COUNT + 1,
        momentum_app_scene_misc_dolphin_dolphin_level_changed,
        app);
    variable_item_set_current_value_index(item, level - 1);
    variable_item_set_current_value_text(item, level_str);

    char xp_str[6];
    snprintf(xp_str, sizeof(xp_str), "%lu", app->dolphin_xp);
    // uin8_t index too small for all levels, use 3 fake items to
    // show buttons and change values in callback
    item = variable_item_list_add(
        var_item_list, "Dolphin XP", 3, momentum_app_scene_misc_dolphin_dolphin_xp_changed, app);
    variable_item_set_current_value_index(
        item,
        app->dolphin_xp == 0              ? 0 :
        app->dolphin_xp == DOLPHIN_MAX_XP ? 2 :
                                            1);
    variable_item_set_current_value_text(item, xp_str);

    char angry_str[4];
    snprintf(angry_str, sizeof(angry_str), "%lu", app->dolphin_angry);
    item = variable_item_list_add(
        var_item_list,
        "Dolphin Angry",
        BUTTHURT_MAX + 1,
        momentum_app_scene_misc_dolphin_dolphin_angry_changed,
        app);
    variable_item_set_current_value_index(item, app->dolphin_angry);
    variable_item_set_current_value_text(item, angry_str);
    variable_item_set_locked(
        item,
        settings.happy_mode,
        "Settings >\n"
        "Desktop >\n"
        "Happy Mode\n"
        "is enabled!");

    item = variable_item_list_add(
        var_item_list,
        "Butthurt Timer",
        COUNT_OF(butthurt_timer_names),
        momentum_app_scene_misc_dolphin_butthurt_timer_changed,
        app);
    value_index = value_index_uint32(
        momentum_settings.butthurt_timer, butthurt_timer_values, COUNT_OF(butthurt_timer_values));
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, butthurt_timer_names[value_index]);
    variable_item_set_locked(
        item,
        settings.happy_mode,
        "Settings >\n"
        "Desktop >\n"
        "Happy Mode\n"
        "is enabled!");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_misc_dolphin_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneMiscDolphin));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_misc_dolphin_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, MomentumAppSceneMiscDolphin, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexDolphinXp:
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneMiscDolphinXp);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_misc_dolphin_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
