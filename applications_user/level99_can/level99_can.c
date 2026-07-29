#include "level99_can_config.h"
#include "level99_can_logger.h"
#include "level99_can_worker.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#define TAG "Level99CAN"

typedef enum {
    Level99CanViewMain,
    Level99CanViewMonitor,
    Level99CanViewWidget,
    Level99CanViewFilters,
    Level99CanViewSettings,
    Level99CanViewTransmit,
    Level99CanViewDiagnostics,
    Level99CanViewTextInput,
} Level99CanView;

typedef enum {
    Level99CanMenuMonitor,
    Level99CanMenuFrameDetails,
    Level99CanMenuFilters,
    Level99CanMenuRecordLog,
    Level99CanMenuSavedLogs,
    Level99CanMenuTransmit,
    Level99CanMenuDiagnostics,
    Level99CanMenuSettings,
    Level99CanMenuAbout,
} Level99CanMenu;

typedef enum {
    Level99CanTransmitId,
    Level99CanTransmitIdType,
    Level99CanTransmitRtr,
    Level99CanTransmitDlc,
    Level99CanTransmitData,
    Level99CanTransmitSend,
} Level99CanTransmitItem;

typedef enum {
    Level99CanTextNone,
    Level99CanTextTransmitId,
    Level99CanTextTransmitData,
    Level99CanTextFilterMin,
    Level99CanTextFilterMax,
} Level99CanTextTarget;

typedef struct {
    Level99CanFrame frames[5];
    size_t count;
    Level99CanDiagnostics diagnostics;
    Level99CanBitrate bitrate;
    Level99CanOscillator oscillator;
    bool capture;
    bool paused;
    bool compact;
    bool logging;
} Level99CanMonitorModel;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    ViewDispatcher* dispatcher;
    Submenu* main_menu;
    View* monitor_view;
    Widget* widget;
    VariableItemList* filters;
    VariableItemList* settings;
    VariableItemList* transmit;
    Submenu* diagnostics_menu;
    TextInput* text_input;
    Level99CanView current_view;
    Level99CanView return_view;
    Level99CanTextTarget text_target;
    char text_buffer[64];
    Level99CanConfig config;
    Level99CanFrame tx_frame;
    Level99CanLogger* logger;
    Level99CanWorker* worker;
    bool capture;
    bool paused;
    bool ready;
} Level99CanApp;

static const uint32_t bitrate_values[] = {10U, 20U, 50U, 100U, 125U, 250U, 500U, 1000U};

static const char* level99_can_mode_name(Level99CanMode mode) {
    switch(mode) {
    case Level99CanModeNormal:
        return "Normal";
    case Level99CanModeSleep:
        return "Sleep";
    case Level99CanModeLoopback:
        return "Loopback";
    case Level99CanModeListenOnly:
        return "Listen-only";
    case Level99CanModeConfiguration:
        return "Config";
    default:
        return "Unknown";
    }
}

static void level99_can_switch(Level99CanApp* app, Level99CanView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void level99_can_show_message(Level99CanApp* app, const char* header, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

static bool level99_can_confirm(
    Level99CanApp* app,
    const char* header,
    const char* text,
    const char* action) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 1, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 27, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Cancel", NULL, action);
    const bool confirmed = dialog_message_show(app->dialogs, message) == DialogMessageButtonRight;
    dialog_message_free(message);
    return confirmed;
}

static void level99_can_monitor_draw(Canvas* canvas, void* model_ptr) {
    Level99CanMonitorModel* model = model_ptr;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "LEVEL99 CAN");
    canvas_set_font(canvas, FontSecondary);
    char line[48];
    snprintf(
        line,
        sizeof(line),
        "%s %luk %uMHz %s E%02X",
        model->capture ? (model->paused ? "PAUSE" : "LIVE") : "STOP",
        (unsigned long)model->bitrate,
        model->oscillator,
        level99_can_mode_name(model->diagnostics.mode),
        model->diagnostics.eflg);
    canvas_draw_str(canvas, 0, 17, line);

    snprintf(
        line,
        sizeof(line),
        "RX:%lu %lu/s Drop:%lu%s",
        (unsigned long)model->diagnostics.received,
        (unsigned long)model->diagnostics.fps,
        (unsigned long)model->diagnostics.dropped,
        model->logging ? " LOG" : "");
    canvas_draw_str(canvas, 0, 26, line);

    const size_t rows = model->compact ? 4U : 2U;
    const size_t first = model->count > rows ? model->count - rows : 0U;
    uint8_t y = 35U;
    for(size_t i = first; i < model->count; i++) {
        const Level99CanFrame* frame = &model->frames[i];
        if(model->compact) {
            snprintf(
                line,
                sizeof(line),
                "%05lu %c%08lX %c%u",
                (unsigned long)(frame->timestamp_ms % 100000U),
                frame->extended ? 'E' : 'S',
                (unsigned long)frame->id,
                frame->rtr ? 'R' : 'D',
                frame->dlc);
        } else {
            int pos = snprintf(
                line,
                sizeof(line),
                "%c%lX %c%u ",
                frame->extended ? 'E' : 'S',
                (unsigned long)frame->id,
                frame->rtr ? 'R' : 'D',
                frame->dlc);
            for(uint8_t b = 0; b < frame->dlc && b < 4U && pos > 0; b++) {
                pos += snprintf(&line[pos], sizeof(line) - (size_t)pos, "%02X ", frame->data[b]);
            }
        }
        canvas_draw_str(canvas, 0, y, line);
        y += model->compact ? 8U : 13U;
    }
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "OK run  UP pause");
}

static bool level99_can_monitor_input(InputEvent* event, void* context) {
    Level99CanApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        app->capture = !app->capture;
        level99_can_worker_set_capture(app->worker, app->capture);
        return true;
    } else if(event->key == InputKeyUp) {
        app->paused = !app->paused;
        level99_can_worker_set_paused(app->worker, app->paused);
        return true;
    } else if(event->key == InputKeyDown) {
        level99_can_worker_clear(app->worker);
        return true;
    } else if(event->key == InputKeyLeft) {
        app->config.compact_display = !app->config.compact_display;
        return true;
    } else if(event->key == InputKeyRight) {
        view_dispatcher_send_custom_event(app->dispatcher, Level99CanMenuFrameDetails);
        return true;
    }
    return false;
}

static void level99_can_refresh_monitor(Level99CanApp* app) {
    with_view_model(
        app->monitor_view,
        Level99CanMonitorModel * model,
        {
            model->count = level99_can_worker_snapshot(
                app->worker, model->frames, COUNT_OF(model->frames), &model->diagnostics);
            model->bitrate = app->config.bitrate;
            model->oscillator = app->config.oscillator;
            model->capture = app->capture;
            model->paused = app->paused;
            model->compact = app->config.compact_display;
            model->logging = level99_can_logger_is_active(app->logger);
        },
        true);
}

static void level99_can_tick(void* context) {
    Level99CanApp* app = context;
    if(app->current_view == Level99CanViewMonitor) level99_can_refresh_monitor(app);
}

static void level99_can_widget_text(Level99CanApp* app, const char* text) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    level99_can_switch(app, Level99CanViewWidget);
}

static void level99_can_show_frame(Level99CanApp* app) {
    Level99CanFrame frame;
    if(!level99_can_worker_latest(app->worker, &frame)) {
        level99_can_show_message(app, "Frame Details", "No captured frame");
        return;
    }
    Level99CanFrame history[LEVEL99_CAN_RING_CAPACITY];
    const size_t history_count =
        level99_can_worker_snapshot(app->worker, history, COUNT_OF(history), NULL);
    uint32_t occurrences = 0U;
    uint32_t previous_timestamp = 0U;
    for(size_t i = 0; i < history_count; i++) {
        if(history[i].id == frame.id && history[i].extended == frame.extended &&
           history[i].rtr == frame.rtr) {
            occurrences++;
            if(history[i].timestamp_ms < frame.timestamp_ms) {
                previous_timestamp = history[i].timestamp_ms;
            }
        }
    }
    FuriString* text = furi_string_alloc_printf(
        "Frame Details\nID: 0x%lX\nType: %s\nTime: %lu ms\nRTR: %s\nDLC: %u\n"
        "Seen: %lu  Delta: %lu ms\nData:",
        (unsigned long)frame.id,
        frame.extended ? "extended" : "standard",
        (unsigned long)frame.timestamp_ms,
        frame.rtr ? "yes" : "no",
        frame.dlc,
        (unsigned long)occurrences,
        (unsigned long)(previous_timestamp ? frame.timestamp_ms - previous_timestamp : 0U));
    for(uint8_t i = 0; i < frame.dlc; i++) {
        furi_string_cat_printf(text, " %02X", frame.data[i]);
    }
    furi_string_cat(text, "\nASCII: ");
    for(uint8_t i = 0; i < frame.dlc; i++) {
        const char c = (frame.data[i] >= 32U && frame.data[i] <= 126U) ? frame.data[i] : '.';
        furi_string_push_back(text, c);
    }
    level99_can_widget_text(app, furi_string_get_cstr(text));
    furi_string_free(text);
}

static void level99_can_show_diagnostics(Level99CanApp* app) {
    Level99CanDiagnostics d;
    level99_can_worker_snapshot(app->worker, NULL, 0U, &d);
    FuriString* text = furi_string_alloc_printf(
        "Diagnostics\nMCP2515: %s\nSPI: %s\nMode: %s\nINT: %s\nEFLG: 0x%02X\n"
        "TEC/REC: %u/%u\nWarn:%u Passive:%u\nBus-off:%u Overflow:%u\n"
        "RX/TX: %lu/%lu\nDrop/log: %lu/%lu\nSPI errors: %lu\n%s",
        d.detected ? "detected" : "NOT DETECTED",
        d.spi_ok ? "OK" : "failed/absent",
        level99_can_mode_name(d.mode),
        d.interrupt_low ? "low" : "high",
        d.eflg,
        d.tec,
        d.rec,
        !!(d.eflg & 0x01U),
        !!(d.eflg & 0x18U),
        !!(d.eflg & 0x20U),
        !!(d.eflg & 0xC0U),
        (unsigned long)d.received,
        (unsigned long)d.transmitted,
        (unsigned long)d.dropped,
        (unsigned long)d.log_dropped,
        (unsigned long)d.spi_errors,
        d.last_error);
    level99_can_widget_text(app, furi_string_get_cstr(text));
    furi_string_free(text);
}

static void level99_can_diagnostics_callback(void* context, uint32_t index) {
    Level99CanApp* app = context;
    if(index == 0U) {
        level99_can_show_diagnostics(app);
        return;
    }
    Level99CanCommand command = {
        .type = index == 1U ? Level99CanCommandReset :
                index == 2U ? Level99CanCommandClearCounters :
                index == 3U ? Level99CanCommandListenOnly :
                              Level99CanCommandLoopbackTest,
    };
    if(!level99_can_worker_command(app->worker, &command)) {
        level99_can_show_message(app, "Diagnostics", "Controller queue unavailable");
    } else {
        level99_can_show_message(
            app,
            "Diagnostics",
            index == 1U ? "Reset queued" :
            index == 2U ? "Counters cleared" :
            index == 3U ? "Listen-only requested" :
                          "Internal loopback queued");
    }
}

static void level99_can_open_text(
    Level99CanApp* app,
    Level99CanTextTarget target,
    Level99CanView return_view,
    const char* header,
    const char* initial) {
    app->text_target = target;
    app->return_view = return_view;
    strlcpy(app->text_buffer, initial, sizeof(app->text_buffer));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    level99_can_switch(app, Level99CanViewTextInput);
}

static void level99_can_text_done(void* context) {
    Level99CanApp* app = context;
    char* end = NULL;
    if(app->text_target == Level99CanTextTransmitId) {
        const uint32_t id = strtoul(app->text_buffer, &end, 0);
        const uint32_t max_id = app->tx_frame.extended ? 0x1FFFFFFFUL : 0x7FFUL;
        if(end != app->text_buffer && *end == '\0' && id <= max_id) {
            app->tx_frame.id = id;
        } else {
            level99_can_show_message(app, "Invalid CAN ID", "Use 0x0..0x7FF or\n0x0..0x1FFFFFFF");
        }
    } else if(app->text_target == Level99CanTextTransmitData) {
        uint8_t count = 0U;
        char* cursor = app->text_buffer;
        while(*cursor && count < 8U) {
            while(*cursor == ' ')
                cursor++;
            if(!*cursor) break;
            char* byte_end;
            const unsigned long value = strtoul(cursor, &byte_end, 16);
            if(byte_end == cursor || value > 0xFFU) {
                count = 0xFFU;
                break;
            }
            app->tx_frame.data[count++] = (uint8_t)value;
            cursor = byte_end;
        }
        while(*cursor == ' ')
            cursor++;
        if(count == 0xFFU || *cursor) {
            level99_can_show_message(app, "Invalid payload", "Enter up to 8 hex bytes");
        } else {
            app->tx_frame.dlc = count;
        }
    } else {
        const uint32_t id = strtoul(app->text_buffer, &end, 0);
        if(end != app->text_buffer && *end == '\0' && id <= 0x1FFFFFFFUL) {
            if(app->text_target == Level99CanTextFilterMin) app->config.filter_id_min = id;
            if(app->text_target == Level99CanTextFilterMax) app->config.filter_id_max = id;
            if(app->config.filter_id_min > app->config.filter_id_max) {
                app->config.filter_id_min = 0U;
                app->config.filter_id_max = 0x1FFFFFFFUL;
                level99_can_show_message(app, "Invalid range", "Filter reset to full range");
            }
        } else {
            level99_can_show_message(app, "Invalid filter ID", "Maximum is 0x1FFFFFFF");
        }
        level99_can_worker_update_config(app->worker, &app->config);
    }
    level99_can_switch(app, app->return_view);
}

static void level99_can_update_transmit_values(Level99CanApp* app) {
    VariableItem* item = variable_item_list_get(app->transmit, Level99CanTransmitIdType);
    variable_item_set_current_value_text(item, app->tx_frame.extended ? "Extended" : "Standard");
    item = variable_item_list_get(app->transmit, Level99CanTransmitRtr);
    variable_item_set_current_value_text(item, app->tx_frame.rtr ? "RTR" : "Data");
    item = variable_item_list_get(app->transmit, Level99CanTransmitDlc);
    char dlc[4];
    snprintf(dlc, sizeof(dlc), "%u", app->tx_frame.dlc);
    variable_item_set_current_value_text(item, dlc);
}

static void level99_can_transmit_changed(VariableItem* item) {
    Level99CanApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_list_get_selected_item_index(app->transmit);
    const uint8_t value = variable_item_get_current_value_index(item);
    if(index == Level99CanTransmitIdType) app->tx_frame.extended = value != 0U;
    if(index == Level99CanTransmitRtr) app->tx_frame.rtr = value != 0U;
    if(index == Level99CanTransmitDlc) app->tx_frame.dlc = value;
    if(!app->tx_frame.extended && app->tx_frame.id > 0x7FFU) app->tx_frame.id = 0x7FFU;
    level99_can_update_transmit_values(app);
}

static void level99_can_transmit_enter(void* context, uint32_t index) {
    Level99CanApp* app = context;
    if(index == Level99CanTransmitId) {
        char id[12];
        snprintf(id, sizeof(id), "0x%lX", (unsigned long)app->tx_frame.id);
        level99_can_open_text(app, Level99CanTextTransmitId, Level99CanViewTransmit, "CAN ID", id);
    } else if(index == Level99CanTransmitData) {
        char data[32] = "";
        size_t pos = 0U;
        for(uint8_t i = 0; i < app->tx_frame.dlc; i++) {
            pos += snprintf(
                &data[pos], sizeof(data) - pos, "%s%02X", i ? " " : "", app->tx_frame.data[i]);
        }
        level99_can_open_text(
            app, Level99CanTextTransmitData, Level99CanViewTransmit, "Hex payload", data);
    } else if(index == Level99CanTransmitSend) {
        if(!level99_can_confirm(
               app, "Confirm one-shot", "Send exactly one validated\nClassic CAN frame?", "SEND")) {
            return;
        }
        Level99CanCommand command = {
            .type = Level99CanCommandTransmit,
            .frame = app->tx_frame,
        };
        if(!level99_can_worker_command(app->worker, &command)) {
            level99_can_show_message(app, "Transmit failed", "Controller queue unavailable");
        } else {
            level99_can_show_message(app, "Transmit", "One-shot request queued");
        }
    }
}

static void level99_can_filter_enter(void* context, uint32_t index) {
    Level99CanApp* app = context;
    char id[12];
    if(index == 3U) {
        snprintf(id, sizeof(id), "0x%lX", (unsigned long)app->config.filter_id_min);
        level99_can_open_text(
            app, Level99CanTextFilterMin, Level99CanViewFilters, "Minimum ID", id);
    } else if(index == 4U) {
        snprintf(id, sizeof(id), "0x%lX", (unsigned long)app->config.filter_id_max);
        level99_can_open_text(
            app, Level99CanTextFilterMax, Level99CanViewFilters, "Maximum ID", id);
    } else if(index == 5U) {
        app->config.filter_enabled = false;
        app->config.filter_id_min = 0U;
        app->config.filter_id_max = 0x1FFFFFFFUL;
        app->config.id_filter = Level99CanIdAny;
        app->config.rtr_filter = Level99CanFrameAny;
        level99_can_worker_update_config(app->worker, &app->config);
        level99_can_show_message(app, "Filters", "Filters cleared");
    } else if(index == 6U) {
        level99_can_config_save(app->storage, &app->config);
        level99_can_show_message(app, "Filters", "Preset saved in config");
    } else if(index == 7U) {
        if(level99_can_config_load(app->storage, &app->config)) {
            VariableItem* item = variable_item_list_get(app->filters, 0U);
            variable_item_set_current_value_index(item, app->config.filter_enabled);
            variable_item_set_current_value_text(item, app->config.filter_enabled ? "On" : "Off");
            item = variable_item_list_get(app->filters, 1U);
            variable_item_set_current_value_index(item, app->config.id_filter);
            static const char* id_labels[] = {"Any", "Standard", "Extended"};
            variable_item_set_current_value_text(item, id_labels[app->config.id_filter]);
            item = variable_item_list_get(app->filters, 2U);
            variable_item_set_current_value_index(item, app->config.rtr_filter);
            static const char* rtr_labels[] = {"Any", "Data", "RTR"};
            variable_item_set_current_value_text(item, rtr_labels[app->config.rtr_filter]);
            level99_can_worker_update_config(app->worker, &app->config);
            level99_can_show_message(app, "Filters", "Saved preset loaded");
        } else {
            level99_can_show_message(app, "Filters", "No valid saved preset");
        }
    }
}

static void level99_can_filter_changed(VariableItem* item) {
    Level99CanApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_list_get_selected_item_index(app->filters);
    const uint8_t value = variable_item_get_current_value_index(item);
    if(index == 0U) {
        app->config.filter_enabled = value != 0U;
        variable_item_set_current_value_text(item, value ? "On" : "Off");
    } else if(index == 1U) {
        app->config.id_filter = value;
        static const char* labels[] = {"Any", "Standard", "Extended"};
        variable_item_set_current_value_text(item, labels[value]);
    } else if(index == 2U) {
        app->config.rtr_filter = value;
        static const char* labels[] = {"Any", "Data", "RTR"};
        variable_item_set_current_value_text(item, labels[value]);
    }
    level99_can_worker_update_config(app->worker, &app->config);
}

static void level99_can_setting_changed(VariableItem* item) {
    Level99CanApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_list_get_selected_item_index(app->settings);
    const uint8_t value = variable_item_get_current_value_index(item);
    if(index == 0U) {
        app->config.oscillator = value ? Level99CanOscillator16MHz : Level99CanOscillator8MHz;
        variable_item_set_current_value_text(item, value ? "16 MHz" : "8 MHz");
    } else if(index == 1U) {
        app->config.bitrate = bitrate_values[value];
        char label[12];
        snprintf(label, sizeof(label), "%lu kbit", (unsigned long)app->config.bitrate);
        variable_item_set_current_value_text(item, label);
    } else if(index == 2U) {
        app->config.default_mode = value == 0U ? Level99CanModeListenOnly : Level99CanModeLoopback;
        variable_item_set_current_value_text(
            item, level99_can_mode_name(app->config.default_mode));
    } else if(index == 3U) {
        app->config.compact_display = value != 0U;
        variable_item_set_current_value_text(item, value ? "Compact" : "Detailed");
    }
    level99_can_config_save(app->storage, &app->config);
    level99_can_worker_update_config(app->worker, &app->config);
    Level99CanCommand command = {.type = Level99CanCommandApplyConfig};
    level99_can_worker_command(app->worker, &command);
}

static void level99_can_menu_callback(void* context, uint32_t index) {
    Level99CanApp* app = context;
    if(index == Level99CanMenuMonitor) {
        level99_can_refresh_monitor(app);
        level99_can_switch(app, Level99CanViewMonitor);
    } else if(index == Level99CanMenuFrameDetails) {
        level99_can_show_frame(app);
    } else if(index == Level99CanMenuFilters) {
        level99_can_switch(app, Level99CanViewFilters);
    } else if(index == Level99CanMenuRecordLog) {
        if(level99_can_logger_is_active(app->logger)) {
            level99_can_logger_stop(app->logger);
            app->config.logging_enabled = false;
            level99_can_config_save(app->storage, &app->config);
            level99_can_show_message(
                app, "CSV logging stopped", level99_can_logger_path(app->logger));
        } else if(level99_can_logger_start(app->logger)) {
            app->config.logging_enabled = true;
            level99_can_config_save(app->storage, &app->config);
            level99_can_show_message(
                app, "CSV logging started", level99_can_logger_path(app->logger));
        } else {
            level99_can_show_message(app, "Logging failed", "Check SD card and storage");
        }
    } else if(index == Level99CanMenuSavedLogs) {
        level99_can_widget_text(
            app,
            "Saved Logs\n/apps_data/level99_can/logs/\n\nOpen CSV files with Archive or qFlipper.");
    } else if(index == Level99CanMenuTransmit) {
        if(level99_can_confirm(
               app,
               "TRANSMIT WARNING",
               "Transmit only on hardware\nand networks you own or are\nexplicitly authorized to test.",
               "NEXT") &&
           level99_can_confirm(
               app,
               "TRANSMIT WARNING",
               "Incorrect CAN traffic can\ncause equipment malfunction\nor unsafe vehicle behavior.",
               "I AGREE")) {
            level99_can_switch(app, Level99CanViewTransmit);
        }
    } else if(index == Level99CanMenuDiagnostics) {
        level99_can_switch(app, Level99CanViewDiagnostics);
    } else if(index == Level99CanMenuSettings) {
        level99_can_switch(app, Level99CanViewSettings);
    } else if(index == Level99CanMenuAbout) {
        level99_can_widget_text(
            app,
            "LEVEL99 CAN v1.0\nClassic CAN / MCP2515\nUnofficial personal fork.\n\n"
            "Built upon CAN Commander concepts where indicated. CAN Commander by Matthew "
            "KuKanich remains separate and unmodified.\n\n3.3V transceiver only. No CAN FD.");
    }
}

static bool level99_can_custom_event(void* context, uint32_t event) {
    Level99CanApp* app = context;
    if(event == Level99CanMenuFrameDetails) {
        level99_can_show_frame(app);
        return true;
    }
    return false;
}

static bool level99_can_navigation(void* context) {
    Level99CanApp* app = context;
    if(app->current_view == Level99CanViewMain) {
        view_dispatcher_stop(app->dispatcher);
    } else if(app->current_view == Level99CanViewTextInput) {
        level99_can_switch(app, app->return_view);
    } else {
        /* One-shot-only transmit has no background repeat state to leak. */
        level99_can_switch(app, Level99CanViewMain);
    }
    return true;
}

static void level99_can_build_menus(Level99CanApp* app) {
    submenu_set_header(app->main_menu, "LEVEL99 CAN");
    static const char* entries[] = {
        "CAN Monitor",
        "Frame Details",
        "Filters",
        "Record Log",
        "Saved Logs",
        "Transmit",
        "Diagnostics",
        "Settings",
        "About",
    };
    for(uint32_t i = 0; i < COUNT_OF(entries); i++) {
        submenu_add_item(app->main_menu, entries[i], i, level99_can_menu_callback, app);
    }

    VariableItem* item =
        variable_item_list_add(app->filters, "Filtering", 2U, level99_can_filter_changed, app);
    variable_item_set_current_value_index(item, app->config.filter_enabled);
    variable_item_set_current_value_text(item, app->config.filter_enabled ? "On" : "Off");
    item = variable_item_list_add(app->filters, "ID type", 3U, level99_can_filter_changed, app);
    variable_item_set_current_value_index(item, app->config.id_filter);
    static const char* id_labels[] = {"Any", "Standard", "Extended"};
    variable_item_set_current_value_text(item, id_labels[app->config.id_filter]);
    item = variable_item_list_add(app->filters, "Frame type", 3U, level99_can_filter_changed, app);
    variable_item_set_current_value_index(item, app->config.rtr_filter);
    static const char* rtr_labels[] = {"Any", "Data", "RTR"};
    variable_item_set_current_value_text(item, rtr_labels[app->config.rtr_filter]);
    variable_item_list_add(app->filters, "Minimum ID", 1U, NULL, app);
    variable_item_list_add(app->filters, "Maximum ID", 1U, NULL, app);
    variable_item_list_add(app->filters, "Clear filters", 1U, NULL, app);
    variable_item_list_add(app->filters, "Save preset", 1U, NULL, app);
    variable_item_list_add(app->filters, "Load preset", 1U, NULL, app);
    variable_item_list_set_enter_callback(app->filters, level99_can_filter_enter, app);

    item =
        variable_item_list_add(app->settings, "Oscillator", 2U, level99_can_setting_changed, app);
    variable_item_set_current_value_index(
        item, app->config.oscillator == Level99CanOscillator16MHz);
    variable_item_set_current_value_text(
        item, app->config.oscillator == Level99CanOscillator16MHz ? "16 MHz" : "8 MHz");
    item = variable_item_list_add(app->settings, "Bitrate", 8U, level99_can_setting_changed, app);
    uint8_t bitrate_index = 0U;
    while(bitrate_index < 7U && bitrate_values[bitrate_index] != (uint32_t)app->config.bitrate)
        bitrate_index++;
    variable_item_set_current_value_index(item, bitrate_index);
    char bitrate_label[12];
    snprintf(bitrate_label, sizeof(bitrate_label), "%lu kbit", (unsigned long)app->config.bitrate);
    variable_item_set_current_value_text(item, bitrate_label);
    item = variable_item_list_add(
        app->settings, "Default mode", 2U, level99_can_setting_changed, app);
    uint8_t mode_index = app->config.default_mode == Level99CanModeListenOnly ? 0U : 1U;
    variable_item_set_current_value_index(item, mode_index);
    variable_item_set_current_value_text(item, level99_can_mode_name(app->config.default_mode));
    item = variable_item_list_add(app->settings, "Display", 2U, level99_can_setting_changed, app);
    variable_item_set_current_value_index(item, app->config.compact_display);
    variable_item_set_current_value_text(
        item, app->config.compact_display ? "Compact" : "Detailed");
    variable_item_list_add(app->settings, "CS pin", 1U, NULL, app);
    variable_item_set_current_value_text(variable_item_list_get(app->settings, 4U), "Pin 4");
    variable_item_list_add(app->settings, "INT pin", 1U, NULL, app);
    variable_item_set_current_value_text(variable_item_list_get(app->settings, 5U), "Pin 6");

    variable_item_list_add(app->transmit, "CAN ID", 1U, NULL, app);
    item = variable_item_list_add(app->transmit, "ID type", 2U, level99_can_transmit_changed, app);
    variable_item_set_current_value_index(item, 0U);
    item = variable_item_list_add(app->transmit, "Frame", 2U, level99_can_transmit_changed, app);
    variable_item_set_current_value_index(item, 0U);
    item = variable_item_list_add(app->transmit, "DLC", 9U, level99_can_transmit_changed, app);
    variable_item_set_current_value_index(item, app->tx_frame.dlc);
    variable_item_list_add(app->transmit, "Payload", 1U, NULL, app);
    variable_item_list_add(app->transmit, "SEND ONE FRAME", 1U, NULL, app);
    variable_item_list_set_enter_callback(app->transmit, level99_can_transmit_enter, app);
    level99_can_update_transmit_values(app);

    submenu_set_header(app->diagnostics_menu, "Diagnostics");
    submenu_add_item(
        app->diagnostics_menu, "Controller Status", 0U, level99_can_diagnostics_callback, app);
    submenu_add_item(
        app->diagnostics_menu, "Reset Controller", 1U, level99_can_diagnostics_callback, app);
    submenu_add_item(
        app->diagnostics_menu, "Clear Counters", 2U, level99_can_diagnostics_callback, app);
    submenu_add_item(
        app->diagnostics_menu, "Enter Listen-Only", 3U, level99_can_diagnostics_callback, app);
    submenu_add_item(
        app->diagnostics_menu, "Run Loopback Test", 4U, level99_can_diagnostics_callback, app);
}

static Level99CanApp* level99_can_alloc(void) {
    Level99CanApp* app = malloc(sizeof(*app));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    level99_can_config_load(app->storage, &app->config);
    app->logger = level99_can_logger_alloc(app->storage);
    app->worker = level99_can_worker_alloc(&app->config, app->logger);
    app->dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->monitor_view = view_alloc();
    app->widget = widget_alloc();
    app->filters = variable_item_list_alloc();
    app->settings = variable_item_list_alloc();
    app->transmit = variable_item_list_alloc();
    app->diagnostics_menu = submenu_alloc();
    app->text_input = text_input_alloc();
    if(!app->logger || !app->worker || !app->dispatcher || !app->main_menu || !app->monitor_view ||
       !app->widget || !app->filters || !app->settings || !app->transmit || !app->text_input ||
       !app->diagnostics_menu) {
        return app;
    }

    app->tx_frame.id = 0x100U;
    app->tx_frame.dlc = 0U;
    app->capture = true;
    view_allocate_model(app->monitor_view, ViewModelTypeLocking, sizeof(Level99CanMonitorModel));
    view_set_context(app->monitor_view, app);
    view_set_draw_callback(app->monitor_view, level99_can_monitor_draw);
    view_set_input_callback(app->monitor_view, level99_can_monitor_input);
    text_input_set_result_callback(
        app->text_input,
        level99_can_text_done,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);

    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, level99_can_custom_event);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, level99_can_navigation);
    view_dispatcher_set_tick_event_callback(app->dispatcher, level99_can_tick, 100U);
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewMain, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->dispatcher, Level99CanViewMonitor, app->monitor_view);
    view_dispatcher_add_view(app->dispatcher, Level99CanViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewFilters, variable_item_list_get_view(app->filters));
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewSettings, variable_item_list_get_view(app->settings));
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewTransmit, variable_item_list_get_view(app->transmit));
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewDiagnostics, submenu_get_view(app->diagnostics_menu));
    view_dispatcher_add_view(
        app->dispatcher, Level99CanViewTextInput, text_input_get_view(app->text_input));
    level99_can_build_menus(app);
    app->ready = true;
    return app;
}

static void level99_can_free(Level99CanApp* app) {
    if(!app) return;
    if(app->logger) level99_can_logger_stop(app->logger);
    if(app->worker) level99_can_worker_free(app->worker);
    if(app->logger) level99_can_logger_free(app->logger);
    if(app->dispatcher && app->ready) {
        for(uint32_t view = Level99CanViewMain; view <= Level99CanViewTextInput; view++) {
            view_dispatcher_remove_view(app->dispatcher, view);
        }
    }
    if(app->text_input) text_input_free(app->text_input);
    if(app->diagnostics_menu) submenu_free(app->diagnostics_menu);
    if(app->transmit) variable_item_list_free(app->transmit);
    if(app->settings) variable_item_list_free(app->settings);
    if(app->filters) variable_item_list_free(app->filters);
    if(app->widget) widget_free(app->widget);
    if(app->monitor_view) view_free(app->monitor_view);
    if(app->main_menu) submenu_free(app->main_menu);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->dialogs) furi_record_close(RECORD_DIALOGS);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t level99_can_app(void* p) {
    UNUSED(p);
    Level99CanApp* app = level99_can_alloc();
    if(!app || !app->ready) {
        level99_can_free(app);
        return -1;
    }
    if(!level99_can_worker_start(app->worker)) {
        level99_can_free(app);
        return -1;
    }
    if(app->config.logging_enabled && !level99_can_logger_start(app->logger)) {
        FURI_LOG_W(TAG, "Saved logging preference could not be started");
    }
    level99_can_switch(app, Level99CanViewMain);
    view_dispatcher_run(app->dispatcher);
    level99_can_config_save(app->storage, &app->config);
    level99_can_free(app);
    return 0;
}
