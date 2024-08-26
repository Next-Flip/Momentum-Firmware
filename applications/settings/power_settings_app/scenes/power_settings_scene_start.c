#include "../power_settings_app.h"

#include <lib/toolbox/value_index.h>

enum PowerSettingsSubmenuIndex {
    PowerSettingsSubmenuIndexBatteryInfo,
    PowerSettingsSubmenuIndexReboot,
    PowerSettingsSubmenuIndexOff,
    PowerSettingsSubmenuShutdownIdle
};

#define SHUTDOWN_IDLE_DELAY_COUNT 9
const char* const shutdown_idle_delay_text[SHUTDOWN_IDLE_DELAY_COUNT] = {
    "OFF",
    "15m",
    "30m",
    "1h",
    "2h",
    "6h",
    "12h",
    "24h",
    "48h",
};
const uint32_t shutdown_idle_delay_value[SHUTDOWN_IDLE_DELAY_COUNT] =
    {0, 900000, 1800000, 3600000, 7200000, 21600000, 43200000, 86400000, 172800000};

static void power_settings_scene_start_auto_lock_delay_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, shutdown_idle_delay_text[index]);
    app->settings.shutdown_idle_delay_ms = shutdown_idle_delay_value[index];
}

static void power_settings_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    PowerSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void power_settings_scene_start_on_enter(void* context) {
    PowerSettingsApp* app = context;
    VariableItemList* variable_item_list = app->variable_item_list;

    VariableItem* item;
    uint8_t value_index;

    variable_item_list_add(variable_item_list, "Battery Info", 1, NULL, NULL);

    variable_item_list_add(variable_item_list, "Reboot", 1, NULL, NULL);

    variable_item_list_add(variable_item_list, "Power OFF", 1, NULL, NULL);

    item = variable_item_list_add(
        variable_item_list,
        "Shutdown on Idle",
        SHUTDOWN_IDLE_DELAY_COUNT,
        power_settings_scene_start_auto_lock_delay_changed,
        app);
    value_index = value_index_uint32(
        app->settings.shutdown_idle_delay_ms,
        shutdown_idle_delay_value,
        SHUTDOWN_IDLE_DELAY_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, shutdown_idle_delay_text[value_index]);

    variable_item_list_set_selected_item(
        variable_item_list,
        scene_manager_get_scene_state(app->scene_manager, PowerSettingsAppSceneStart));

    variable_item_list_set_enter_callback(
        variable_item_list, power_settings_scene_start_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PowerSettingsAppViewVariableItemList);
}

bool power_settings_scene_start_on_event(void* context, SceneManagerEvent event) {
    PowerSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PowerSettingsSubmenuIndexBatteryInfo) {
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppSceneBatteryInfo);
        } else if(event.event == PowerSettingsSubmenuIndexReboot) {
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppSceneReboot);
        } else if(event.event == PowerSettingsSubmenuIndexOff) {
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppScenePowerOff);
        } else if(event.event == PowerSettingsSubmenuShutdownIdle) {
        }
        scene_manager_set_scene_state(app->scene_manager, PowerSettingsAppSceneStart, event.event);
        consumed = true;
    }
    return consumed;
}

void power_settings_scene_start_on_exit(void* context) {
    PowerSettingsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
