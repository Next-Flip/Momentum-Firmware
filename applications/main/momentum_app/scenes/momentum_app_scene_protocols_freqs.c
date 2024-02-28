#include "../momentum_app.h"

enum VarItemListIndex {
    VarItemListIndexUseDefaults,
    VarItemListIndexStaticFreqs,
    VarItemListIndexHopperFreqs,
};

void momentum_app_scene_protocols_freqs_var_item_list_callback(void* context, uint32_t index) {
    MomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void momentum_app_scene_protocols_freqs_use_defaults_changed(VariableItem* item) {
    MomentumApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    app->subghz_use_defaults = value;
    app->save_subghz_freqs = true;
}

void momentum_app_scene_protocols_freqs_on_enter(void* context) {
    MomentumApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list,
        "Use Defaults",
        2,
        momentum_app_scene_protocols_freqs_use_defaults_changed,
        app);
    variable_item_set_current_value_index(item, app->subghz_use_defaults);
    variable_item_set_current_value_text(item, app->subghz_use_defaults ? "ON" : "OFF");

    item = variable_item_list_add(var_item_list, "Static Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Hopper Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    variable_item_list_set_enter_callback(
        var_item_list, momentum_app_scene_protocols_freqs_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MomentumAppSceneProtocolsFreqs));

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewVarItemList);
}

bool momentum_app_scene_protocols_freqs_on_event(void* context, SceneManagerEvent event) {
    MomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, MomentumAppSceneProtocolsFreqs, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexStaticFreqs:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneProtocolsFreqsStatic, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsFreqsStatic);
            break;
        case VarItemListIndexHopperFreqs:
            scene_manager_set_scene_state(
                app->scene_manager, MomentumAppSceneProtocolsFreqsHopper, 0);
            scene_manager_next_scene(app->scene_manager, MomentumAppSceneProtocolsFreqsHopper);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void momentum_app_scene_protocols_freqs_on_exit(void* context) {
    MomentumApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
