#include "../nfc_app_i.h"
#include "../helpers/saflok.h"

#include <bit_lib.h>

typedef struct {
    uint8_t level_num;
    char* short_level_name;
    char* level_name;
} SaflokKeyLevel;

static SaflokKeyLevel key_levels[] = {
    {1, "Guest", "Guest Key"},
    {2, "Cnectors", "Connectors"},
    {3, "Suite", "Suite"},
    {4, "LmtdUse", "Limited Use"},
    {5, "Failsafe", "Failsafe"},
    {6, "Inhibit", "Inhibit"},
    {7, "MtgMstr", "Pool/Meeting Master"},
    {8, "Hsekpng", "Housekeeping"},
    {9, "FloorKey", "Floor Key"},
    {10, "SctnKey", "Section Key"},
    {11, "RmsMstr", "Rooms Master"},
    {12, "GrndMstr", "Grand Master"},
    {13, "Emrgncy", "Emergency"},
    {14, "Lockout", "Electronic Lockout"},
    {15, "SecProg", "Secondary Programming Key"},
    {16, "PriProg", "Primary Programming Key"},
};

static const char* options[] = {
    "Card Level",
    "Card Type",
    "Card ID",
    "Opening Key",
    "Lock ID",
    "Pass #",
    "Seq & Comb",
    "Deadbolt Overide",
    "Restricted Days",
    "Property ID",
    "Creation",
    "Expiration",
    "Done",
};

static const char* days_of_the_week[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
};

void set_state_flag(NfcApp* app, NfcSceneSaflokState flag, bool new_state);
void number_input_callback(void* context, int32_t number);
void submenu_item_callback(void* context, uint32_t index);
void variable_item_list_update_value(NfcApp* app, VariableItem* item, uint32_t value, bool apply);
void variable_item_list_enter_callback(void* context, uint32_t index);
void variable_item_list_change_callback(VariableItem* item);

void date_time_done_callback(void* context) {
    NfcApp* app = context;

    set_state_flag(app, NfcSceneSaflokStateInSubView, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
}

void date_time_input_callback(void* context) {
    NfcApp* app = context;

    // Expire date cannot be before creation date because
    //   it's stored, unsigned, relative to creation
    uint32_t expire = datetime_datetime_to_timestamp(&app->nfc_saflok_data->expire);
    uint32_t creation = datetime_datetime_to_timestamp(&app->nfc_saflok_data->creation);
    expire = MAX(expire, creation);
    datetime_timestamp_to_datetime(expire, &app->nfc_saflok_data->expire);

    // Expire year cannot be more than 15 years after creation
    //   date because it's stored as a 4-bit unsigned int
    app->nfc_saflok_data->expire.year =
        MIN(app->nfc_saflok_data->expire.year, app->nfc_saflok_data->creation.year + 15);

    // Trigger callback for both dates to update their labels
    for(uint8_t i = 10; i < 12; i++) {
        VariableItem* item = variable_item_list_get(app->variable_item_list, i);
        variable_item_list_update_value(app, item, 0, false);
    }
}

void set_state_flag(NfcApp* app, NfcSceneSaflokState flag, bool new_state) {
    NfcSceneSaflokState state =
        scene_manager_get_scene_state(app->scene_manager, NfcSceneSaflokInput);
    if(new_state)
        state |= flag;
    else
        state &= ~flag;
    scene_manager_set_scene_state(app->scene_manager, NfcSceneSaflokInput, state);
}

void number_input_callback(void* context, int32_t number) {
    NfcApp* app = context;

    // Find the selected item and trigger the callback to update its label
    uint8_t item_index = variable_item_list_get_selected_item_index(app->variable_item_list);
    VariableItem* item = variable_item_list_get(app->variable_item_list, item_index);
    variable_item_list_update_value(app, item, number, true);

    set_state_flag(app, NfcSceneSaflokStateInSubView, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
}

void submenu_item_callback(void* context, uint32_t index) {
    NfcApp* app = context;

    // Find the selected item
    uint8_t item_index = variable_item_list_get_selected_item_index(app->variable_item_list);
    VariableItem* item = variable_item_list_get(app->variable_item_list, item_index);

    switch(item_index) {
    case 8: // Restricted Days
        if(index == COUNT_OF(days_of_the_week)) {
            // Done button
            set_state_flag(app, NfcSceneSaflokStateInSubView, false);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
        } else {
            // Toggle the day's bit and update the label
            char state = (app->nfc_saflok_data->restricted_days ^= 1 << index);
            state = state ? 'X' : ' ';

            FuriString* label = furi_string_alloc();
            furi_string_printf(label, "[%c] %s", state, days_of_the_week[index]);
            submenu_change_item_label(app->submenu, index, furi_string_get_cstr(label));
            furi_string_free(label);
        }
        break;
        // All other options just select a single item from the list
    default:
        variable_item_set_current_value_index(item, index);
        set_state_flag(app, NfcSceneSaflokStateInSubView, false);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
    }

    // Always update the Variable Item List label
    variable_item_list_update_value(app, item, index, true);
}

void variable_item_list_update_value(NfcApp* app, VariableItem* item, uint32_t value, bool apply) {
    uint8_t key = 255;
    for(size_t i = 0; i < COUNT_OF(options); i++) {
        if(variable_item_list_get(app->variable_item_list, i) == item) {
            key = i;
            break;
        }
    }
    if(key >= COUNT_OF(options)) return;

    FuriString* value_text = furi_string_alloc();
    switch(key) {
    case 0: // Card Level
        if(apply)
            app->nfc_saflok_data->card_level = value;
        else
            value = app->nfc_saflok_data->card_level;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, COUNT_OF(key_levels));
        furi_string_printf(value_text, "%s", key_levels[value].short_level_name);
        break;

    case 1: // Card Type
        if(apply)
            app->nfc_saflok_data->card_type = value;
        else
            value = app->nfc_saflok_data->card_type;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, 16); // 4-bit value
        furi_string_printf(value_text, "%ld", value);
        break;
    case 2: // Card ID
        if(apply)
            app->nfc_saflok_data->card_id = value;
        else
            value = app->nfc_saflok_data->card_id;
        furi_string_printf(value_text, "%ld", value);
        break;
    case 3: // Opening Key
        if(apply)
            app->nfc_saflok_data->opening_key = value;
        else
            value = app->nfc_saflok_data->opening_key;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, 4); // 2-bit value
        furi_string_printf(value_text, "%ld", value);
        break;
    case 4: // Lock ID
        if(apply)
            app->nfc_saflok_data->lock_id = value;
        else
            value = app->nfc_saflok_data->lock_id;
        furi_string_printf(value_text, "%ld", value);
        break;
    case 5: // Pass #
        if(apply)
            app->nfc_saflok_data->pass_number = value;
        else
            value = app->nfc_saflok_data->pass_number;
        furi_string_printf(value_text, "%ld", value);
        break;
    case 6: // Seq & Comb
        if(apply)
            app->nfc_saflok_data->sequence_and_combination = value;
        else
            value = app->nfc_saflok_data->sequence_and_combination;
        furi_string_printf(value_text, "%ld", value);
        break;

    case 7: // Deadbolt Overide
        if(apply)
            app->nfc_saflok_data->deadbolt_override = value;
        else
            value = app->nfc_saflok_data->deadbolt_override;

        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, 2);

        if(value) {
            furi_string_printf(value_text, "On");
        } else {
            furi_string_printf(value_text, "Off");
        }
        break;

    case 8: // Restricted Days
        uint8_t num_restricted = 0;
        for(uint8_t i = 0; i < 7; i++) {
            if(app->nfc_saflok_data->restricted_days & (1 << i)) num_restricted++;
        }

        furi_string_printf(value_text, "%d/7", num_restricted);
        break;

    case 9: // Property ID
        if(apply)
            app->nfc_saflok_data->property_id = value;
        else
            value = app->nfc_saflok_data->property_id;

        furi_string_printf(value_text, "%ld", value);
        break;

    case 10: // Creation
        furi_string_printf(
            value_text,
            "%04d-%02d-%02d %02d:%02d",
            app->nfc_saflok_data->creation.year,
            app->nfc_saflok_data->creation.month,
            app->nfc_saflok_data->creation.day,
            app->nfc_saflok_data->creation.hour,
            app->nfc_saflok_data->creation.minute);
        break;
    case 11: // Expiration
        furi_string_printf(
            value_text,
            "%04d-%02d-%02d %02d:%02d",
            app->nfc_saflok_data->expire.year,
            app->nfc_saflok_data->expire.month,
            app->nfc_saflok_data->expire.day,
            app->nfc_saflok_data->expire.hour,
            app->nfc_saflok_data->expire.minute);
        break;

    case 12: // Done
        break;
    }
    variable_item_set_current_value_text(item, furi_string_get_cstr(value_text));
    furi_string_free(value_text);
}

void variable_item_list_enter_callback(void* context, uint32_t index) {
    NfcApp* app = context;

    // Reset submenu
    submenu_reset(app->submenu);

    // Some options use a Submenu, others use a NumberInput
    bool number_input = false;
    int32_t number_input_max = 0;
    int32_t number_input_current = 0;

    switch(index) {
    case 0: // Card Level
        for(size_t i = 0; i < COUNT_OF(key_levels); i++) {
            submenu_add_item(
                app->submenu, key_levels[i].level_name, i, submenu_item_callback, context);
        }
        submenu_set_selected_item(app->submenu, app->nfc_saflok_data->card_level);
        break;

    case 1: // Card Type
        number_input = true;
        number_input_current = app->nfc_saflok_data->card_type;
        number_input_max = 15;
        break;
    case 2: // Card ID
        number_input = true;
        number_input_current = app->nfc_saflok_data->card_id;
        number_input_max = 255;
        break;
    case 3: // Opening Key
        number_input = true;
        number_input_current = app->nfc_saflok_data->opening_key;
        number_input_max = 3;
        break;
    case 4: // Lock ID
        number_input = true;
        number_input_current = app->nfc_saflok_data->lock_id;
        number_input_max = 16383;
        break;
    case 5: // Pass #
        number_input = true;
        number_input_current = app->nfc_saflok_data->pass_number;
        number_input_max = 4095;
        break;
    case 6: // Seq & Comb
        number_input = true;
        number_input_current = app->nfc_saflok_data->sequence_and_combination;
        number_input_max = 4095;
        break;

    case 7: // Deadbolt Override
        // This option can't be clicked (it's just a boolean, there's no need)
        return;

    case 8: // Restricted Days
        FuriString* label = furi_string_alloc();
        for(size_t i = 0; i < COUNT_OF(days_of_the_week); i++) {
            char state = app->nfc_saflok_data->restricted_days & (1 << i);
            state = state ? 'X' : ' ';

            furi_string_reset(label);
            furi_string_printf(label, "[%c] %s", state, days_of_the_week[i]);
            submenu_add_item(
                app->submenu, furi_string_get_cstr(label), i, submenu_item_callback, context);
        }
        furi_string_free(label);

        submenu_add_item(
            app->submenu, "Done", COUNT_OF(days_of_the_week), submenu_item_callback, context);
        submenu_set_selected_item(app->submenu, 0);
        break;

    case 9: // Property ID
        number_input = true;
        number_input_current = app->nfc_saflok_data->property_id;
        number_input_max = 4095;
        break;

    case 10: // Creation
        date_time_input_set_result_callback(
            app->date_time_input,
            date_time_input_callback,
            date_time_done_callback,
            context,
            &app->nfc_saflok_data->creation);

        set_state_flag(app, NfcSceneSaflokStateInSubView, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewDateTimeInput);

        return; // Don't switch to other subview
    case 11: // Expire
        date_time_input_set_result_callback(
            app->date_time_input,
            date_time_input_callback,
            date_time_done_callback,
            context,
            &app->nfc_saflok_data->expire);

        set_state_flag(app, NfcSceneSaflokStateInSubView, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewDateTimeInput);

        return; // Don't switch to other subview
    case 12: // Done
        view_dispatcher_send_custom_event(app->view_dispatcher, SceneManagerEventTypeCustom);
        break;
    }

    // Switch to the appropriate view
    if(number_input) {
        number_input_set_header_text(app->number_input, options[index]);
        number_input_set_result_callback(
            app->number_input,
            number_input_callback,
            context,
            number_input_current,
            0,
            number_input_max);
        set_state_flag(app, NfcSceneSaflokStateInSubView, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewNumberInput);
    } else {
        submenu_set_header(app->submenu, options[index]);
        set_state_flag(app, NfcSceneSaflokStateInSubView, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewMenu);
    }
}

void variable_item_list_change_callback(VariableItem* item) {
    NfcApp* app = variable_item_get_context(item);
    uint8_t value = variable_item_get_current_value_index(item);

    uint8_t key = 255;
    for(size_t i = 0; i < COUNT_OF(options); i++) {
        if(variable_item_list_get(app->variable_item_list, i) == item) {
            key = i;
            break;
        }
    }
    if(key >= COUNT_OF(options)) return;

    variable_item_list_update_value(app, item, value, true);
}

void nfc_scene_saflok_input_on_enter(void* context) {
    NfcApp* app = context;
    VariableItem* item;

    NfcSceneSaflokState state =
        scene_manager_get_scene_state(app->scene_manager, NfcSceneSaflokInput);
    // Check if we're making a new card
    if((state & NfcSceneSaflokStateEditCard) == 0) {
        // Reset all fields to 0
        app->nfc_saflok_data->card_level = 0;
        app->nfc_saflok_data->card_type = 0;
        app->nfc_saflok_data->card_id = 0;
        app->nfc_saflok_data->opening_key = 0;
        app->nfc_saflok_data->lock_id = 0;
        app->nfc_saflok_data->pass_number = 0;
        app->nfc_saflok_data->sequence_and_combination = 0;
        app->nfc_saflok_data->deadbolt_override = 0;
        app->nfc_saflok_data->restricted_days = 0;
        app->nfc_saflok_data->property_id = 0;

        // Set creation date/time to now
        DateTime* datetime = malloc(sizeof(DateTime));
        furi_hal_rtc_get_datetime(datetime);
        app->nfc_saflok_data->creation.year = datetime->year;
        app->nfc_saflok_data->creation.month = datetime->month;
        app->nfc_saflok_data->creation.day = datetime->day;
        app->nfc_saflok_data->creation.hour = datetime->hour;
        app->nfc_saflok_data->creation.minute = datetime->minute;

        // Set expiration date/time to a week from now
        // Convert to timestamp and back to let datetime.c
        //   handle days per month and leap years and such
        uint32_t timestamp = datetime_datetime_to_timestamp(datetime);
        timestamp += 60 /*secs*/ * 60 /*mins*/ * 24 /*hours*/ * 7 /*days*/;
        datetime_timestamp_to_datetime(timestamp, datetime);

        app->nfc_saflok_data->expire.year = datetime->year;
        app->nfc_saflok_data->expire.month = datetime->month;
        app->nfc_saflok_data->expire.day = datetime->day;
        app->nfc_saflok_data->expire.hour = datetime->hour;
        app->nfc_saflok_data->expire.minute = datetime->minute;

        // Switch scene state to edit so we don't erase everything if we return
        state |= NfcSceneSaflokStateEditCard;
        scene_manager_set_scene_state(app->scene_manager, NfcSceneSaflokInput, state);

        // Select the first item
        variable_item_list_set_selected_item(app->variable_item_list, 0);
    }

    for(size_t i = 0; i < COUNT_OF(options); i++) {
        item = variable_item_list_add(
            app->variable_item_list, options[i], 0, variable_item_list_change_callback, context);
        variable_item_set_current_value_index(item, 0);
        variable_item_list_update_value(app, item, 0, false);
    }

    variable_item_list_set_enter_callback(
        app->variable_item_list, variable_item_list_enter_callback, context);

    set_state_flag(app, NfcSceneSaflokStateInSubView, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
}

bool nfc_scene_saflok_input_on_event(void* context, SceneManagerEvent event) {
    NfcApp* app = context;
    bool consumed = false;

    NfcSceneSaflokState state =
        scene_manager_get_scene_state(app->scene_manager, NfcSceneSaflokInput);

    if(event.type == SceneManagerEventTypeBack) {
        if(state & NfcSceneSaflokStateInSubView) {
            set_state_flag(app, NfcSceneSaflokStateInSubView, false);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        // Generate the actual data from the input values
        saflok_generate_mf_classic(app->nfc_device, app->nfc_saflok_data);
        set_state_flag(app, NfcSceneSaflokStateInSubView, false);
        scene_manager_next_scene(app->scene_manager, NfcSceneReadMenu);
        consumed = true;
    }
    return consumed;
}

void nfc_scene_saflok_input_on_exit(void* context) {
    NfcApp* app = context;

    // Clear view
    variable_item_list_reset(app->variable_item_list);
    submenu_reset(app->submenu);
}
