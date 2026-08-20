#include "../power_settings_app.h"

#include <lib/toolbox/value_index.h>

enum PowerSettingsSubmenuIndex {
    PowerSettingsSubmenuIndexBatteryInfo,
    PowerSettingsSubmenuIndexReboot,
    PowerSettingsSubmenuIndexOff,
    PowerSettingsSubmenuIndexAutoPowerOff,
};

#define AUTO_POWEROFF_DELAY_COUNT 12
const char* const auto_poweroff_delay_text[AUTO_POWEROFF_DELAY_COUNT] =
    {"5min", "10min", "15min", "30min", "45min", "60min", "90min", "2h", "6h", "12h", "24h", "48h"};

const uint32_t auto_poweroff_delay_value[AUTO_POWEROFF_DELAY_COUNT] = {
    300000,
    600000,
    900000,
    1800000,
    2700000,
    3600000,
    5400000,
    7200000,
    21600000,
    43200000,
    86400000,
    172800000};

#define AUTO_POWEROFF_PERCENT_STEP 5
#define AUTO_POWEROFF_PERCENT_MAX  95
#define CHARGE_SUPRESS_STEP        5

static const char* const auto_poweroff_mode_text[3] = {
    "OFF",
    "Timer",
    "%",
};

static const char* const power_off_timeout_text[3] = {
    [PowerOffTimeout30] = "30s",
    [PowerOffTimeout60] = "60s",
    [PowerOffTimeout90] = "90s",
};

// change variable_item_list visible text and charge_supress_percent_settings when user change item in variable_item_list
static void power_settings_scene_start_charge_supress_percent_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint32_t value = (variable_item_get_current_value_index(item) + 1) * CHARGE_SUPRESS_STEP;
    char charge_supress_str[6];
    snprintf(charge_supress_str, sizeof(charge_supress_str), "%lu%%", value);

    variable_item_set_current_value_text(item, charge_supress_str);
    app->settings.charge_supress_percent = value == 100 ? 0 : value;
}

// change variable_item_list visible text and auto_poweroff_mode_settings when user change item in variable_item_list
static void power_settings_scene_start_auto_poweroff_mode_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->settings.auto_poweroff_mode = (PowerAutoPoweroffMode)index;
    variable_item_set_current_value_text(item, auto_poweroff_mode_text[index]);

    // Force a rebuild so the Duration / Percentage sub-item swaps in/out.
    scene_manager_set_scene_state(
        app->scene_manager, PowerSettingsAppSceneStart, PowerSettingsSubmenuIndexAutoPowerOff);
    power_settings_scene_start_on_enter(app);
}

// change variable_item_list visible text and app_poweroff_delay_time_settings when user change item in variable_item_list
static void power_settings_scene_start_auto_poweroff_delay_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, auto_poweroff_delay_text[index]);
    app->settings.auto_poweroff_delay_ms = auto_poweroff_delay_value[index];
}

// change variable_item_list visible text and auto_poweroff_percent_settings when user change item in variable_item_list
static void power_settings_scene_start_auto_poweroff_percent_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint8_t value = (variable_item_get_current_value_index(item) + 1) * AUTO_POWEROFF_PERCENT_STEP;
    char buf[6];
    snprintf(buf, sizeof(buf), "%u%%", value);

    variable_item_set_current_value_text(item, buf);
    app->settings.auto_poweroff_percent = value;
}

// change variable_item_list visible text and power_off_timeout_settings when user change item in variable_item_list
static void power_settings_scene_start_power_off_timeout_changed(VariableItem* item) {
    PowerSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->settings.power_off_timeout = (PowerOffTimeout)index;
    variable_item_set_current_value_text(item, power_off_timeout_text[index]);
}

static void power_settings_scene_start_submenu_callback(void* context, uint32_t index) {
    //show selected menu screen by index
    furi_assert(context);
    PowerSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void power_settings_scene_start_on_enter(void* context) {
    PowerSettingsApp* app = context;
    VariableItemList* variable_item_list = app->variable_item_list;

    VariableItem* item;
    uint8_t value_index;

    variable_item_list_reset(variable_item_list);

    variable_item_list_add(variable_item_list, "Battery Info", 1, NULL, NULL);

    variable_item_list_add(variable_item_list, "Reboot", 1, NULL, NULL);

    variable_item_list_add(variable_item_list, "Power OFF", 1, NULL, NULL);

    item = variable_item_list_add(
        variable_item_list,
        "Auto PowerOff",
        3,
        power_settings_scene_start_auto_poweroff_mode_changed,
        app);

    variable_item_set_current_value_index(item, app->settings.auto_poweroff_mode);
    variable_item_set_current_value_text(
        item, auto_poweroff_mode_text[app->settings.auto_poweroff_mode]);

    if(app->settings.auto_poweroff_mode == PowerAutoPoweroffModeTimer) {
        item = variable_item_list_add(
            variable_item_list,
            "Duration",
            AUTO_POWEROFF_DELAY_COUNT,
            power_settings_scene_start_auto_poweroff_delay_changed,
            app);

        value_index = value_index_uint32(
            app->settings.auto_poweroff_delay_ms,
            auto_poweroff_delay_value,
            AUTO_POWEROFF_DELAY_COUNT);
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, auto_poweroff_delay_text[value_index]);
    } else if(app->settings.auto_poweroff_mode == PowerAutoPoweroffModePercent) {
        item = variable_item_list_add(
            variable_item_list,
            "Percentage",
            AUTO_POWEROFF_PERCENT_MAX / AUTO_POWEROFF_PERCENT_STEP,
            power_settings_scene_start_auto_poweroff_percent_changed,
            app);

        uint8_t pct = app->settings.auto_poweroff_percent;
        if(pct < AUTO_POWEROFF_PERCENT_STEP) pct = AUTO_POWEROFF_PERCENT_STEP;
        if(pct > AUTO_POWEROFF_PERCENT_MAX) pct = AUTO_POWEROFF_PERCENT_MAX;
        value_index = (pct / AUTO_POWEROFF_PERCENT_STEP) - 1;
        char buf[6];
        snprintf(buf, sizeof(buf), "%u%%", pct);
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(
        variable_item_list,
        "Warning Timeout",
        3,
        power_settings_scene_start_power_off_timeout_changed,
        app);

    variable_item_set_current_value_index(item, app->settings.power_off_timeout);
    variable_item_set_current_value_text(
        item, power_off_timeout_text[app->settings.power_off_timeout]);

    item = variable_item_list_add(
        variable_item_list,
        "Limit Charge",
        100 / CHARGE_SUPRESS_STEP,
        power_settings_scene_start_charge_supress_percent_changed,
        app);

    uint8_t value =
        app->settings.charge_supress_percent == 0 ? 100 : app->settings.charge_supress_percent;
    value_index = (value / CHARGE_SUPRESS_STEP) - 1;
    char charge_supress_str[6];
    snprintf(charge_supress_str, sizeof(charge_supress_str), "%u%%", value);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, charge_supress_str);

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
        switch(event.event) {
        case PowerSettingsSubmenuIndexBatteryInfo:
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppSceneBatteryInfo);
            break;
        case PowerSettingsSubmenuIndexReboot:
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppSceneReboot);
            break;
        case PowerSettingsSubmenuIndexOff:
            scene_manager_next_scene(app->scene_manager, PowerSettingsAppScenePowerOff);
            break;
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
