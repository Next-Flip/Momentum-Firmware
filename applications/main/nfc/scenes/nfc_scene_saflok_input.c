#include "../nfc_app_i.h"
#include "../helpers/saflok.h"

#include <bit_lib.h>

typedef struct {
    char* short_level_name;
    char* level_name;
} SaflokCardLevel;

static SaflokCardLevel card_levels[] = {
    {"Guest", "Guest Key"},
    {"Cnectors", "Connectors"},
    {"Suite", "Suite"},
    {"LmtdUse", "Limited Use"},
    {"Failsafe", "Failsafe"},
    {"Inhibit", "Inhibit"},
    {"MtgMstr", "Pool/Meeting Master"},
    {"Hsekpng", "Housekeeping"},
    {"FloorKey", "Floor Key"},
    {"SctnKey", "Section Key"},
    {"RmsMstr", "Rooms Master"},
    {"GrndMstr", "Grand Master"},
    {"Emrgncy", "Emergency"},
    {"Lockout", "Electronic Lockout"},
    {"SecProg", "Secondary Programming Key"},
    {"PriProg", "Primary Programming Key"},
};

typedef struct {
    char* short_type_name;
    char* type_name;
    char* ppk_short_type_name;
    char* ppk_type_name;
} SaflokCardType;

static SaflokCardType card_types[] = {
    {"Stndard", "Make Standard Key", "Stndard", "Make Standard Key"},
    {"Reseque", "Make Resequencing Key", "LED Diag", "LED Diagnostic"},
    {"Block", "Make Block Key", "E2 Chng", "Dis/Enable E2 Changes"},
    {"Unblock", "Make Unblock Key", "Erase E2", "Erase Lock (E2) Memory"},
    {"ChgDate", "Change Checkout Date", "Batt Disc", "Battery Disconect"},
    {"Chkout", "Check Out a Key", "Display", "Make Display Key"},
    {"CnclPrer", "Cancel Prereg Key", "", ""},
    {"ChknPrer", "Check In Prereg Key", "Intero", "Make Interrogation Key"},
    {"Cancel ID", "Make Cancel-A-Key-ID", "", ""},
    {"Chng Rm", "Make Change Room Key", "", ""},
};

static bool available_card_types[][COUNT_OF(card_types)] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Guest Key
    {1, 1, 1, 1, 1, 1, 0, 0, 1, 1}, // Connectors
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Suite
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Limited Use
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, // Failsafe
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Inhibit
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Pool/Meeting Master
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Housekeeping
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Floor Key
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Section Key
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Rooms Master
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Grand Master
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Emergency
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 0}, // Electronic Lockout
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Secondary Programming Key
    {1, 1, 1, 1, 1, 1, 0, 1, 0, 0}, // Primary Programming Key
};

typedef struct {
    char* short_key_name;
    char* key_name;
} SaflokDisplayKey;

static SaflokDisplayKey display_key_types[] = {
    {"EPR Ver", "EPROM Version"},
    {"Clk Time", "Clock Time"},
    {"Clk Date", "Clock Date"},
    {"AutoLtc", "AutoLatch Status"},
    {"LstRcrds", "Last 2 LPI Records"},
    {"KnbSwt", "Knob Switch Status"},
    {"DBltSwt", "Dead Bolt Switch Status"},
    {"MtrSwt", "Motor Switch + Latch State"},
    {"LowBtry", "Low Battery Status"},
    {"Clck Run", "Clock Run Test"},
    {"LEDTest", "LED Lights Test"},
};

static const char* options[] = {
    "Card Level",
    "Card Type",
    "Card ID",
    "Opening Key",
    "Key/Lock ID",
    "Pass #/Areas",
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
    // Pass #/Areas
    case 5:
        if(index == 12) {
            // Done button
            set_state_flag(app, NfcSceneSaflokStateInSubView, false);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewVariableItemList);
        } else {
            // Toggle the area's bit and update the label
            char state = (app->nfc_saflok_data->pass_number ^= 1 << index);
            state = state ? 'X' : ' ';

            FuriString* label = furi_string_alloc();
            furi_string_printf(label, "[%c] Area %ld", state, index + 1);
            submenu_change_item_label(app->submenu, index, furi_string_get_cstr(label));
            furi_string_free(label);
        }
        break;
    // Restricted Days
    case 8:
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
    for(uint8_t i = 0; i < COUNT_OF(options); i++) {
        if(variable_item_list_get(app->variable_item_list, i) == item) {
            key = i;
            break;
        }
    }
    if(key >= COUNT_OF(options)) return;

    NfcSceneSaflokState state =
        scene_manager_get_scene_state(app->scene_manager, NfcSceneSaflokInput);

    FuriString* value_text = furi_string_alloc();
    switch(key) {
    case 0: // Card Level
        if(apply)
            app->nfc_saflok_data->card_level = value;
        else
            value = app->nfc_saflok_data->card_level;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, COUNT_OF(card_levels));
        furi_string_printf(value_text, "%s", card_levels[value].short_level_name);

        // Update available card types
        VariableItem* _card_type_item = variable_item_list_get(app->variable_item_list, 1);
        if(_card_type_item)
            variable_item_list_update_value(
                app, _card_type_item, app->nfc_saflok_data->card_type, true);
        break;

    case 1: // Card Type
        // Enforce available_card_types list by skipping over unavailable types
        uint32_t last_value = app->nfc_saflok_data->card_type;
        bool direction = value > last_value;
        while(!available_card_types[app->nfc_saflok_data->card_level][value]) {
            // Prevent overflow/underflow
            if(value >= COUNT_OF(card_types)) direction = 0;
            if(value == 0) direction = 1;

            // Skip type
            if(direction)
                value++;
            else
                value--;
        }

        // Find # of highest available card type
        uint8_t max_value = 0;
        for(uint8_t i = 0; i < COUNT_OF(card_types); i++)
            if(available_card_types[app->nfc_saflok_data->card_level][i]) max_value = i;

        if(apply)
            app->nfc_saflok_data->card_type = value;
        else
            value = app->nfc_saflok_data->card_type;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, max_value + 1);

        if(app->nfc_saflok_data->card_level != 15) {
            furi_string_printf(value_text, "%s", card_types[value].short_type_name);
        } else {
            furi_string_printf(value_text, "%s", card_types[value].ppk_short_type_name);

            // Reset card ID to ID=0 if Display Key was just selected
            if(last_value != 5 && value == 5) app->nfc_saflok_data->card_id = 0;
        }

        // Update available card IDs
        VariableItem* _card_id_item = variable_item_list_get(app->variable_item_list, 2);
        if(_card_id_item)
            variable_item_list_update_value(
                app, _card_id_item, app->nfc_saflok_data->card_id, true);

        break;
    case 2: // Card ID
        if(apply)
            app->nfc_saflok_data->card_id = value;
        else
            value = app->nfc_saflok_data->card_id;
        if(app->nfc_saflok_data->card_level == 15 && app->nfc_saflok_data->card_type == 5) {
            // Is a display key, use display key list for title
            variable_item_set_current_value_index(item, value);
            variable_item_set_values_count(item, COUNT_OF(display_key_types));
            furi_string_printf(value_text, "%s", display_key_types[value].short_key_name);
        } else {
            variable_item_set_values_count(item, 0);
            furi_string_printf(value_text, "%ld", value);
        }
        break;
    case 3: // Opening Key
        if(apply)
            app->nfc_saflok_data->opening_key = value;
        else
            value = app->nfc_saflok_data->opening_key;
        variable_item_set_current_value_index(item, value);
        variable_item_set_values_count(item, 2);

        if(value) {
            furi_string_printf(value_text, "On");
        } else {
            furi_string_printf(value_text, "Off");
        }
        break;
    case 4: // Key/Lock ID
        if(apply)
            app->nfc_saflok_data->lock_id = value;
        else
            value = app->nfc_saflok_data->lock_id;
        furi_string_printf(value_text, "%ld", value);
        break;
    case 5: // Pass #/Areas
        uint8_t num_selected = 0;
        for(uint8_t i = 0; i < 12; i++) {
            if(app->nfc_saflok_data->pass_number & (1 << i)) num_selected++;
        }

        // Make a right arrow always show on the list item
        variable_item_set_current_value_index(item, 0);
        variable_item_set_values_count(item, 2);

        // If we're on the main view and tried to change to value=1,
        //   the user pressed the right arrow, so enter the subview
        if(!(state & NfcSceneSaflokStateInSubView) && apply && value == 1)
            variable_item_list_enter_callback(app, 5);

        furi_string_printf(value_text, "%d/12", num_selected);
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

        // Make a right arrow always show on the list item
        variable_item_set_current_value_index(item, 0);
        variable_item_set_values_count(item, 2);

        // If we're on the main view and tried to change to value=1,
        //   the user pressed the right arrow, so enter the subview
        if(!(state & NfcSceneSaflokStateInSubView) && apply && value == 1)
            variable_item_list_enter_callback(app, 8);

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
    bool use_number_input = false;
    bool use_date_time_input = false;
    int32_t number_input_max = 0;
    int32_t number_input_current = 0;
    uint8_t submenu_selected_item = 0;
    FuriString* label = furi_string_alloc();

    switch(index) {
    case 0: // Card Level
        for(uint8_t i = 0; i < COUNT_OF(card_levels); i++) {
            submenu_add_item(
                app->submenu, card_levels[i].level_name, i, submenu_item_callback, context);
        }
        submenu_selected_item = app->nfc_saflok_data->card_level;
        break;
    case 1: // Card Type
        bool is_ppk = app->nfc_saflok_data->card_level == 15;
        for(uint8_t i = 0; i < COUNT_OF(card_types); i++) {
            if(available_card_types[app->nfc_saflok_data->card_level][i])
                submenu_add_item(
                    app->submenu,
                    is_ppk ? card_types[i].ppk_type_name : card_types[i].type_name,
                    i,
                    submenu_item_callback,
                    context);
        }
        submenu_selected_item = app->nfc_saflok_data->card_type;
        break;

    case 2: // Card ID
        if(app->nfc_saflok_data->card_level == 15 && app->nfc_saflok_data->card_type == 5) {
            // Is display key, show submenu instead
            for(uint8_t i = 0; i < COUNT_OF(display_key_types); i++) {
                submenu_add_item(
                    app->submenu, display_key_types[i].key_name, i, submenu_item_callback, context);
            }
            submenu_selected_item = app->nfc_saflok_data->card_id;
        } else {
            use_number_input = true;
            number_input_current = app->nfc_saflok_data->card_id;
            number_input_max = 255;
        }
        break;
    case 3: // Opening Key
        // This option can't be clicked (it's just a boolean, there's no need)
        return;

    case 4: // Key/Lock ID
        use_number_input = true;
        number_input_current = app->nfc_saflok_data->lock_id;
        number_input_max = 16383;
        break;
    case 5: // Pass #/Areas
        for(uint8_t i = 0; i < 12; i++) {
            char state = app->nfc_saflok_data->pass_number & (1 << i);
            state = state ? 'X' : ' ';

            furi_string_reset(label);
            furi_string_printf(label, "[%c] Area %d", state, i + 1);
            submenu_add_item(
                app->submenu, furi_string_get_cstr(label), i, submenu_item_callback, context);
        }

        submenu_add_item(app->submenu, "Done", 12, submenu_item_callback, context);
        break;
    case 6: // Seq & Comb
        use_number_input = true;
        number_input_current = app->nfc_saflok_data->sequence_and_combination;
        number_input_max = 4095;
        break;

    case 7: // Deadbolt Override
        // This option can't be clicked (it's just a boolean, there's no need)
        return;

    case 8: // Restricted Days
        for(uint8_t i = 0; i < COUNT_OF(days_of_the_week); i++) {
            char state = app->nfc_saflok_data->restricted_days & (1 << i);
            state = state ? 'X' : ' ';

            furi_string_reset(label);
            furi_string_printf(label, "[%c] %s", state, days_of_the_week[i]);
            submenu_add_item(
                app->submenu, furi_string_get_cstr(label), i, submenu_item_callback, context);
        }

        submenu_add_item(
            app->submenu, "Done", COUNT_OF(days_of_the_week), submenu_item_callback, context);
        break;

    case 9: // Property ID
        use_number_input = true;
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

        use_date_time_input = true;
        break;
    case 11: // Expire
        date_time_input_set_result_callback(
            app->date_time_input,
            date_time_input_callback,
            date_time_done_callback,
            context,
            &app->nfc_saflok_data->expire);

        use_date_time_input = true;
        break;
    case 12: // Done
        view_dispatcher_send_custom_event(app->view_dispatcher, SceneManagerEventTypeCustom);
        break;
    }
    furi_string_free(label);

    // Switch to the appropriate view
    set_state_flag(app, NfcSceneSaflokStateInSubView, true);
    if(use_number_input) {
        number_input_set_header_text(app->number_input, options[index]);
        number_input_set_result_callback(
            app->number_input,
            number_input_callback,
            context,
            number_input_current,
            0,
            number_input_max);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewNumberInput);
    } else if(use_date_time_input) {
        date_time_input_set_editable_fields(
            app->date_time_input,
            true,
            true,
            true,
            true,
            true,
            // Prevent editing seconds to make it clear they aren't used
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewDateTimeInput);
    } else {
        submenu_set_header(app->submenu, options[index]);
        submenu_set_selected_item(app->submenu, submenu_selected_item);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcViewMenu);
    }
}

void variable_item_list_change_callback(VariableItem* item) {
    NfcApp* app = variable_item_get_context(item);
    uint8_t value = variable_item_get_current_value_index(item);

    uint8_t key = 255;
    for(uint8_t i = 0; i < COUNT_OF(options); i++) {
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

    for(uint8_t i = 0; i < COUNT_OF(options); i++) {
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
