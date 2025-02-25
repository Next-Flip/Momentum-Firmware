#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexAssetPack,
    VarItemListIndexPackWarning,
    VarItemListIndexAnimSpeed,
    VarItemListIndexCycleAnims,
    VarItemListIndexUnlockAnims,
};

static bool check_asset_pack_folders(const char* pack_name) {
    if(pack_name == NULL) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc();
    bool result = false;

    furi_string_printf(path, "%s/%s/Icons", ASSET_PACKS_PATH, pack_name);
    if(storage_dir_exists(storage, furi_string_get_cstr(path))) result = true;
    if(!result) {
        furi_string_printf(path, "%s/%s/Fonts", ASSET_PACKS_PATH, pack_name);
        if(storage_dir_exists(storage, furi_string_get_cstr(path))) result = true;
    }

    furi_string_free(path);
    return result;
}

void momentum_app_scene_interface_graphics_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void momentum_app_scene_interface_graphics_asset_pack_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, index == 0 ? "Default" : *CharList_get(app->asset_pack_names, index - 1));
    strlcpy(
        momentum_settings.asset_pack,
        index == 0 ? "" : *CharList_get(app->asset_pack_names, index - 1),
        ASSET_PACKS_NAME_LEN);
    app->asset_pack_index = index;
    app->save_settings = true;
    app->apply_pack = true;

    // Workaround to force scene reload to rebuild the list with/without pack warning and
    // return to 0 index ("Asset Pack").
    scene_manager_previous_scene(app->scene_manager);
    scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceGraphics);
    scene_manager_set_scene_state(app->scene_manager, MomentumAppSceneInterfaceGraphics, 0);
}

const char* const anim_speed_names[] = {
    "25%",
    "50%",
    "75%",
    "100%",
    "125%",
    "150%",
    "175%",
    "200%",
    "225%",
    "250%",
    "275%",
    "300%",
};
const uint32_t anim_speed_values[COUNT_OF(anim_speed_names)] = {
    25,
    50,
    75,
    100,
    125,
    150,
    175,
    200,
    225,
    250,
    275,
    300,
};
static void momentum_app_scene_interface_graphics_anim_speed_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, anim_speed_names[index]);
    momentum_settings.anim_speed = anim_speed_values[index];
    app->save_settings = true;
}

const char* const cycle_anims_names[] = {
    "OFF",
    "Meta.txt",
    "15 S",
    "30 S",
    "1 M",
    "2 M",
    "5 M",
    "10 M",
    "15 M",
    "30 M",
    "1 H",
    "2 H",
    "6 H",
    "12 H",
    "24 H",
};
const int32_t cycle_anims_values[COUNT_OF(cycle_anims_names)] = {
    -1,
    0,
    15,
    30,
    60,
    120,
    300,
    600,
    900,
    1800,
    3600,
    7200,
    21600,
    43200,
    86400,
};
static void momentum_app_scene_interface_graphics_cycle_anims_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, cycle_anims_names[index]);
    momentum_settings.cycle_anims = cycle_anims_values[index];
    app->save_settings = true;
}

static void momentum_app_scene_interface_graphics_unlock_anims_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    momentum_settings.unlock_anims = value;
    app->save_settings = true;
}

void momentum_app_scene_interface_graphics_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    const char* selected_asset_pack =
        app->asset_pack_index == 0 ?
            "Default" :
            *CharList_get(app->asset_pack_names, app->asset_pack_index - 1);
    item = variable_item_list_add(
        var_item_list,
        "Asset Pack",
        CharList_size(app->asset_pack_names) + 1,
        momentum_app_scene_interface_graphics_asset_pack_changed,
        app);
    variable_item_set_current_value_index(item, app->asset_pack_index);
    variable_item_set_current_value_text(item, selected_asset_pack);

    if(app->asset_pack_index > 0) {
        if(check_asset_pack_folders(selected_asset_pack)) {
            item = variable_item_list_add(var_item_list, "Size Warning", 0, NULL, app);
            variable_item_set_current_value_text(item, ">");
        }
    }

    item = variable_item_list_add(
        var_item_list,
        "Anim Speed",
        COUNT_OF(anim_speed_names),
        momentum_app_scene_interface_graphics_anim_speed_changed,
        app);
    value_index = value_index_uint32(
        momentum_settings.anim_speed, anim_speed_values, COUNT_OF(anim_speed_values));
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, anim_speed_names[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Cycle Anims",
        COUNT_OF(cycle_anims_names),
        momentum_app_scene_interface_graphics_cycle_anims_changed,
        app);
    value_index = value_index_int32(
        momentum_settings.cycle_anims, cycle_anims_values, COUNT_OF(cycle_anims_values));
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cycle_anims_names[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Unlock Anims",
        2,
        momentum_app_scene_interface_graphics_unlock_anims_changed,
        app);
    variable_item_set_current_value_index(item, momentum_settings.unlock_anims);
    variable_item_set_current_value_text(item, momentum_settings.unlock_anims ? "ON" : "OFF");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_interface_graphics_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneInterfaceGraphics));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_interface_graphics_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, MomentumAppSceneInterfaceGraphics, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexAssetPack:
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneInterfaceGraphicsPack);
            break;
        case VarItemListIndexPackWarning:
            scene_manager_next_scene(
                app->scene_manager, MomentumAppSceneInterfaceGraphicsPackWarning);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_interface_graphics_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
