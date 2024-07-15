#include "menu.h"

#include <gui/elements.h>
#include <assets_icons.h>
#include <gui/icon_i.h>
#include <gui/icon_animation_i.h>
#include <gui/canvas_i.h>
#include <dolphin/dolphin_i.h>
#include <dolphin/helpers/dolphin_state.h>
#include <furi.h>
#include <furi_hal.h>
#include <m-array.h>
#include <momentum/momentum.h>
#include <m-string.h>

struct Menu {
    View* view;

    FuriTimer* scroll_timer;
};

typedef struct {
    const char* label;
    IconAnimation* icon;
    uint32_t index;
    MenuItemCallback callback;
    void* callback_context;
} MenuItem;

ARRAY_DEF(MenuItemArray, MenuItem, M_POD_OPLIST);

#define M_OPL_MenuItemArray_t() ARRAY_OPLIST(MenuItemArray, M_POD_OPLIST)

typedef struct {
    MenuItemArray_t items;
    size_t position;

    size_t scroll_counter;
    size_t vertical_offset;
} MenuModel;

static void menu_process_up(Menu* menu);
static void menu_process_down(Menu* menu);
static void menu_process_left(Menu* menu);
static void menu_process_right(Menu* menu);
static void menu_process_ok(Menu* menu);

static void menu_get_name(MenuItem* item, FuriString* name, bool shorter) {
    furi_string_set(name, item->label);
    if(shorter) {
        if(!furi_string_cmp(name, "Momentum")) {
            furi_string_set(name, "MNTM");
            return;
        } else if(!furi_string_cmp(name, "125 kHz RFID")) {
            furi_string_set(name, "RFID");
            return;
        } else if(!furi_string_cmp(name, "Sub-GHz")) {
            furi_string_set(name, "SubGHz");
            return;
        }
    }
    if(furi_string_start_with_str(name, "[")) {
        size_t trim = furi_string_search_str(name, "] ", 1);
        if(trim != FURI_STRING_FAILURE) {
            furi_string_right(name, trim + 2);
        }
    }
}

static void menu_centered_icon(
    Canvas* canvas,
    MenuItem* item,
    size_t x,
    size_t y,
    size_t width,
    size_t height) {
    canvas_draw_icon_animation(
        canvas,
        x + (width - item->icon->icon->width) / 2,
        y + (height - item->icon->icon->height) / 2,
        item->icon);
}

static size_t menu_scroll_counter(MenuModel* model, bool selected) {
    if(!selected) return 0;
    size_t scroll_counter = model->scroll_counter;
    if(scroll_counter > 0) {
        scroll_counter--;
    }
    return scroll_counter;
}

static void menu_draw_callback(Canvas* canvas, void* _model) {
    MenuModel* model = _model;

    canvas_clear(canvas);

    size_t position = model->position;
    size_t items_count = MenuItemArray_size(model->items);
    if(items_count) {
        MenuItem* item;
        size_t shift_position;
        FuriString* name = furi_string_alloc();
        switch(momentum_settings.menu_style) {
        case MenuStyleList: {
            for(uint8_t i = 0; i < 3; i++) {
                canvas_set_font(canvas, i == 1 ? FontPrimary : FontSecondary);
                shift_position = (position + items_count + i - 1) % items_count;
                item = MenuItemArray_get(model->items, shift_position);
                menu_centered_icon(canvas, item, 4, 3 + 22 * i, 14, 14);
                menu_get_name(item, name, false);
                size_t scroll_counter = menu_scroll_counter(model, i == 1);
                elements_scrollable_text_line(
                    canvas, 22, 14 + 22 * i, 98, name, scroll_counter, false);
            }
            // Frame and scrollbar
            elements_frame(canvas, 0, 21, 128 - 5, 21);
            elements_scrollbar(canvas, position, items_count);
            break;
        }
        case MenuStyleWii: {
            if(items_count > 6 && position >= 4) {
                if(position >= items_count - 2 + (items_count % 2)) {
                    shift_position = position - (position % 2) - 4;
                } else {
                    shift_position = position - (position % 2) - 2;
                }
            } else {
                shift_position = 0;
            }
            canvas_set_font(canvas, FontSecondary);
            size_t item_i;
            size_t x_off, y_off;
            for(uint8_t i = 0; i < 6; i++) {
                item_i = shift_position + i;
                if(item_i >= items_count) continue;
                x_off = (i / 2) * 43 + 1;
                y_off = (i % 2) * 32;
                bool selected = item_i == position;
                if(selected) {
                    elements_slightly_rounded_box(canvas, 0 + x_off, 0 + y_off, 40, 30);
                    canvas_set_color(canvas, ColorWhite);
                }
                item = MenuItemArray_get(model->items, item_i);
                menu_centered_icon(canvas, item, x_off, y_off, 40, 20);
                menu_get_name(item, name, true);
                size_t scroll_counter = menu_scroll_counter(model, selected);
                elements_scrollable_text_line_centered(
                    canvas, 20 + x_off, 26 + y_off, 36, name, scroll_counter, false, true);
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                } else {
                    elements_frame(canvas, 0 + x_off, 0 + y_off, 40, 30);
                }
            }
            break;
        }
        case MenuStyleDsi: {
            for(int8_t i = -2; i <= 2; i++) {
                shift_position = (position + items_count + i) % items_count;
                item = MenuItemArray_get(model->items, shift_position);
                size_t width = 24;
                size_t height = 26;
                int32_t pos_x = 64;
                int32_t pos_y = 36;
                if(i == 0) {
                    width += 6;
                    height += 4;
                    elements_bold_rounded_frame(
                        canvas, pos_x - width / 2, pos_y - height / 2, width, height + 5);
                    canvas_set_font(canvas, FontBatteryPercent);
                    canvas_draw_str_aligned(
                        canvas, pos_x - 9, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "S");
                    canvas_draw_str_aligned(
                        canvas, pos_x, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "TAR");
                    canvas_draw_str_aligned(
                        canvas, pos_x + 9, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "T");

                    canvas_draw_rframe(canvas, 0, 0, 128, 18, 3);
                    canvas_draw_line(canvas, 60, 18, 64, 26);
                    canvas_draw_line(canvas, 64, 26, 68, 18);
                    canvas_set_color(canvas, ColorWhite);
                    canvas_draw_line(canvas, 60, 17, 68, 17);
                    canvas_draw_box(canvas, 62, 21, 5, 2);
                    canvas_set_color(canvas, ColorBlack);

                    canvas_set_font(canvas, FontPrimary);
                    menu_get_name(item, name, false);
                    size_t scroll_counter = menu_scroll_counter(model, true);
                    elements_scrollable_text_line_centered(
                        canvas,
                        pos_x,
                        pos_y - height / 2 - 8,
                        126,
                        name,
                        scroll_counter,
                        false,
                        true);
                } else {
                    pos_x += (width + 6) * i;
                    pos_y += 2;
                    elements_slightly_rounded_frame(
                        canvas, pos_x - width / 2, pos_y - height / 2, width, height);
                }
                menu_centered_icon(canvas, item, pos_x - 7, pos_y - 7, 14, 14);
            }
            elements_scrollbar_horizontal(canvas, 0, 64, 128, position, items_count);
            break;
        }
        case MenuStylePs4: {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 1, 1, AlignLeft, AlignTop, furi_hal_version_get_name_ptr());
            char str[10];
            Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
            snprintf(str, 10, "Level %i", dolphin_get_level(dolphin->state->data.icounter));
            furi_record_close(RECORD_DOLPHIN);
            canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, str);
            for(int8_t i = -1; i <= 4; i++) {
                shift_position = position + i;
                if(shift_position >= items_count) continue;
                item = MenuItemArray_get(model->items, shift_position);
                size_t width = 20;
                size_t height = 20;
                size_t pos_x = 36;
                size_t pos_y = 27;
                if(i == 0) {
                    width += 10;
                    height += 10;
                    pos_y += 2;
                    canvas_draw_box(canvas, pos_x - width / 2, pos_y + height / 2, width, 9);
                    canvas_set_color(canvas, ColorWhite);
                    canvas_set_font(canvas, FontBatteryPercent);
                    canvas_draw_str_aligned(
                        canvas, pos_x, pos_y + height / 2 + 1, AlignCenter, AlignTop, "Start");

                    canvas_set_color(canvas, ColorBlack);
                    canvas_set_font(canvas, FontSecondary);
                    menu_get_name(item, name, true);
                    size_t scroll_counter = menu_scroll_counter(model, true);
                    elements_scrollable_text_line(
                        canvas,
                        pos_x + width / 2 + 2,
                        pos_y + height / 2 + 7,
                        74,
                        name,
                        scroll_counter,
                        false);
                } else {
                    pos_x += (width + 1) * i + (i < 0 ? -6 : 6);
                }
                canvas_draw_frame(canvas, pos_x - width / 2, pos_y - height / 2, width, height);
                menu_centered_icon(canvas, item, pos_x - 7, pos_y - 7, 14, 14);
            }
            elements_scrollbar_horizontal(canvas, 0, 64, 128, position, items_count);
            break;
        }
        case MenuStyleVertical: {
            canvas_set_orientation(canvas, CanvasOrientationVertical);
            shift_position = model->vertical_offset;
            if(shift_position >= position || shift_position + 7 <= position) {
                // In case vertical_offset is out of sync due to changing menu styles
                shift_position = CLAMP(
                    MAX((int32_t)position - 4, 0),
                    MAX((int32_t)MenuItemArray_size(model->items) - 8, 0),
                    0);
                model->vertical_offset = shift_position;
            }
            canvas_set_font(canvas, FontSecondary);
            size_t item_i;
            size_t y_off;
            for(size_t i = 0; i < 8; i++) {
                item_i = shift_position + i;
                if(item_i >= items_count) continue;
                y_off = 16 * i;
                bool selected = item_i == position;
                if(selected) {
                    elements_slightly_rounded_box(canvas, 0, y_off, 64, 16);
                    canvas_set_color(canvas, ColorWhite);
                }
                item = MenuItemArray_get(model->items, item_i);
                menu_centered_icon(canvas, item, 0, y_off, 16, 16);
                menu_get_name(item, name, true);
                size_t scroll_counter = menu_scroll_counter(model, selected);
                elements_scrollable_text_line(
                    canvas, 17, y_off + 12, 46, name, scroll_counter, false);
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                }
            }
            canvas_set_orientation(canvas, CanvasOrientationHorizontal);
            break;
        }
        case MenuStyleC64: {
            size_t index;
            size_t y_off, x_off;

            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 0, AlignCenter, AlignTop, "* FLIPPADORE 64 BASIC *");

            char memstr[29];
            snprintf(memstr, sizeof(memstr), "%d BASIC BYTES FREE", memmgr_get_free_heap());
            canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignTop, memstr);

            canvas_set_font(canvas, FontKeyboard);

            for(size_t i = 0; i < 2; i++) {
                for(size_t j = 0; j < 5; j++) {
                    index = i * 5 + j + (position - (position % 10));
                    if(index >= items_count) continue;
                    y_off = (9 * j) + 13;
                    x_off = 64 * i;
                    bool selected = index == position;
                    size_t scroll_counter = menu_scroll_counter(model, selected);
                    if(selected) {
                        canvas_draw_box(canvas, x_off, y_off + 4, 64, 9);
                        canvas_set_color(canvas, ColorWhite);
                    }
                    item = MenuItemArray_get(model->items, index);
                    menu_get_name(item, name, true);

                    char indexstr[5];
                    snprintf(indexstr, sizeof(indexstr), "%d.", index);
                    furi_string_replace_at(name, 0, 0, indexstr);

                    elements_scrollable_text_line(
                        canvas, x_off + 2, y_off + 12, 60, name, scroll_counter, false);

                    if(selected) {
                        canvas_set_color(canvas, ColorBlack);
                    }
                }
            }

            break;
        }
        case MenuStyleCompact: {
            size_t index;
            size_t y_off, x_off;

            canvas_set_font(canvas, FontBatteryPercent);

            for(size_t i = 0; i < 2; i++) {
                for(size_t j = 0; j < 8; j++) {
                    index = i * 8 + j + (position - (position % 16));
                    if(index >= items_count) continue;
                    y_off = (8 * j);
                    x_off = 64 * i;
                    bool selected = index == position;
                    size_t scroll_counter = menu_scroll_counter(model, selected);
                    if(selected) {
                        canvas_draw_box(canvas, x_off, y_off, 64, 8);
                        canvas_set_color(canvas, ColorWhite);
                    }
                    item = MenuItemArray_get(model->items, index);
                    menu_get_name(item, name, true);

                    elements_scrollable_text_line(
                        canvas, x_off + 1, y_off + 7, 62, name, scroll_counter, false);

                    if(selected) {
                        canvas_set_color(canvas, ColorBlack);
                    }
                }
            }

            break;
        }
        case MenuStyleMNTM: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_icon(canvas, 62, 4, &I_Release_arrow_18x15);
            canvas_draw_line(canvas, 5, 15, 59, 15);
            canvas_draw_line(canvas, 7, 17, 61, 17);
            canvas_draw_line(canvas, 10, 19, 63, 19);
            char title[20];
            snprintf(title, sizeof(title), "%s", furi_hal_version_get_name_ptr());
            canvas_draw_str(canvas, 5, 12, title);
            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);
            uint8_t hour = curr_dt.hour;
            uint8_t min = curr_dt.minute;
            if(hour > 12) {
                hour -= 12;
            }
            if(hour == 0) {
                hour = 12;
            }
            canvas_set_font(canvas, FontSecondary);
            char clk[20];
            snprintf(clk, sizeof(clk), "%02u:%02u", hour, min);
            canvas_draw_str(canvas, 5, 34, clk);

            uint32_t battery_capacity = furi_hal_power_get_battery_full_capacity();
            uint32_t battery_remaining = furi_hal_power_get_battery_remaining_capacity();
            bool ext5v = furi_hal_power_is_otg_enabled();
            uint16_t battery_percent = (battery_remaining * 100) / battery_capacity;
            bool charge_state = false;

            // Determine charge state
            if(furi_hal_power_is_charging()) {
                if(battery_percent < 100 && !furi_hal_power_is_charging_done()) {
                    charge_state = true;
                }
            }

            // Display battery percentage
            char bat_display[20];
            snprintf(bat_display, sizeof(bat_display), "%d%%", battery_percent);
            canvas_draw_str(canvas, 5, 45, bat_display);

            // Display charge state icon
            if(charge_state) {
                canvas_draw_icon(canvas, 28, 33, &I_Voltage_16x16);
            }

            // Display OTG state
            char ext5v_display[20];
            snprintf(ext5v_display, sizeof(ext5v_display), "5v: %s", ext5v ? "On" : "Off");
            canvas_draw_str(canvas, 5, 56, ext5v_display);

            MenuItem* item = MenuItemArray_get(model->items, position);
            menu_get_name(item, name, true);
            elements_bold_rounded_frame(canvas, 42, 23, 35, 33);
            menu_centered_icon(canvas, item, 43, 24, 35, 32);
            canvas_draw_frame(canvas, 0, 0, 128, 64);

            uint8_t startY = 15;
            uint8_t itemHeight = 10;
            uint8_t itemMaxVisible = 5;
            size_t endItem = position + itemMaxVisible;
            endItem = (endItem > MenuItemArray_size(model->items)) ?
                          MenuItemArray_size(model->items) :
                          endItem;

            for(size_t i = position; i < endItem; i++) {
                MenuItem* item = MenuItemArray_get(model->items, i);
                menu_get_name(item, name, true);
                uint8_t yPos = startY + ((i - position) * itemHeight);
                size_t scroll_counter = menu_scroll_counter(model, i == position);
                elements_scrollable_text_line(canvas, 83, yPos, 43, name, scroll_counter, false);
            }
            break;
        }
        default:
            break;
        }

        furi_string_free(name);
    } else {
        canvas_draw_str(canvas, 2, 32, "Empty");
        elements_scrollbar(canvas, 0, 0);
    }
}

static bool menu_input_callback(InputEvent* event, void* context) {
    Menu* menu = context;
    bool consumed = true;
    if(momentum_settings.menu_style == MenuStyleVertical &&
       furi_hal_rtc_is_flag_set(FuriHalRtcFlagHandOrient)) {
        if(event->key == InputKeyLeft) {
            event->key = InputKeyRight;
        } else if(event->key == InputKeyRight) {
            event->key = InputKeyLeft;
        }
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            menu_process_up(menu);
            break;
        case InputKeyDown:
            menu_process_down(menu);
            break;
        case InputKeyLeft:
            menu_process_left(menu);
            break;
        case InputKeyRight:
            menu_process_right(menu);
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat) {
                menu_process_ok(menu);
            }
            break;
        default:
            consumed = false;
            break;
        }
    } else {
        consumed = false;
    }

    return consumed;
}

static void menu_scroll_timer_callback(void* context) {
    Menu* menu = context;
    with_view_model(menu->view, MenuModel * model, { model->scroll_counter++; }, true);
}

static void menu_enter(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(MenuItemArray_size(model->items)) {
                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_start(item->icon);
            }
            model->scroll_counter = 0;
        },
        true);
    furi_timer_start(menu->scroll_timer, 333);
}

static void menu_exit(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(MenuItemArray_size(model->items)) {
                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_stop(item->icon);
            }
        },
        false);
    furi_timer_stop(menu->scroll_timer);
}

Menu* menu_alloc(void) {
    Menu* menu = malloc(sizeof(Menu));
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(MenuModel));
    view_set_draw_callback(menu->view, menu_draw_callback);
    view_set_input_callback(menu->view, menu_input_callback);
    view_set_enter_callback(menu->view, menu_enter);
    view_set_exit_callback(menu->view, menu_exit);

    menu->scroll_timer = furi_timer_alloc(menu_scroll_timer_callback, FuriTimerTypePeriodic, menu);

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            MenuItemArray_init(model->items);
            model->position = 0;
        },
        true);

    return menu;
}

void menu_free(Menu* menu) {
    furi_check(menu);

    menu_reset(menu);
    with_view_model(menu->view, MenuModel * model, { MenuItemArray_clear(model->items); }, false);
    view_free(menu->view);
    furi_timer_free(menu->scroll_timer);

    free(menu);
}

View* menu_get_view(Menu* menu) {
    furi_check(menu);
    return menu->view;
}

void menu_add_item(
    Menu* menu,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback,
    void* context) {
    furi_check(menu);
    furi_check(label);

    MenuItem* item = NULL;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            item = MenuItemArray_push_new(model->items);
            item->label = label;
            item->icon = icon ? icon_animation_alloc(icon) : icon_animation_alloc(&A_Plugins_14);
            view_tie_icon_animation(menu->view, item->icon);
            item->index = index;
            item->callback = callback;
            item->callback_context = context;
        },
        true);
}

void menu_reset(Menu* menu) {
    furi_check(menu);
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            for
                M_EACH(item, model->items, MenuItemArray_t) {
                    icon_animation_stop(item->icon);
                    icon_animation_free(item->icon);
                }

            MenuItemArray_reset(model->items);
            model->position = 0;
        },
        true);
}

static void menu_set_position(Menu* menu, uint32_t position) {
    furi_check(menu);

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(position < MenuItemArray_size(model->items) && position != model->position) {
                model->scroll_counter = 0;

                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_stop(item->icon);

                item = MenuItemArray_get(model->items, position);
                icon_animation_start(item->icon);

                model->position = position;
            }
        },
        true);
}

uint32_t menu_get_selected_item(Menu* menu) {
    furi_check(menu);

    uint32_t selected_item_index = 0;

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(model->position < MenuItemArray_size(model->items)) {
                const MenuItem* item = MenuItemArray_cget(model->items, model->position);
                selected_item_index = item->index;
            }
        },
        false);

    return selected_item_index;
}

void menu_set_selected_item(Menu* menu, uint32_t index) {
    furi_check(menu);

    with_view_model(
        menu->view,
        MenuModel * model,
        {
            size_t position = 0;
            MenuItemArray_it_t it;
            for(MenuItemArray_it(it, model->items); !MenuItemArray_end_p(it);
                MenuItemArray_next(it)) {
                if(index == MenuItemArray_cref(it)->index) {
                    break;
                }
                position++;
            }

            const size_t items_size = MenuItemArray_size(model->items);

            if(position >= items_size) {
                position = 0;
            }

            model->position = position;
        },
        true);
}

static void menu_process_up(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(momentum_settings.menu_style) {
            case MenuStyleList:
            case MenuStyleMNTM:
                if(position > 0) {
                    position--;
                } else {
                    position = count - 1;
                }
                break;
            case MenuStyleWii:
                if(position % 2 || (position == count - 1 && count % 2)) {
                    position--;
                } else {
                    position++;
                }
                break;
            case MenuStyleC64:
            case MenuStyleCompact:
                if(position > 0) {
                    position--;
                } else {
                    position = count - 1;
                }
                break;

            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_down(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(momentum_settings.menu_style) {
            case MenuStyleList:
            case MenuStyleMNTM:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;
            case MenuStyleWii:
                if(position % 2 || (position == count - 1 && count % 2)) {
                    position--;
                } else {
                    position++;
                }
                break;
            case MenuStyleC64:
            case MenuStyleCompact:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;

            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_left(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(momentum_settings.menu_style) {
            case MenuStyleWii:
                if(position < 2) {
                    if(count % 2) {
                        position = count - 1;
                    } else {
                        position = count - 2 + position % 2;
                    }
                } else {
                    position -= 2;
                }
                break;
            case MenuStyleDsi:
            case MenuStylePs4:
            case MenuStyleVertical:
                size_t vertical_offset = model->vertical_offset;
                if(position > 0) {
                    position--;
                    if(vertical_offset && vertical_offset == position) {
                        vertical_offset--;
                    }
                } else {
                    position = count - 1;
                    vertical_offset = count - 8;
                }
                model->vertical_offset = vertical_offset;
                break;
            case MenuStyleC64:
                if((position % 10) < 5) {
                    position = position + 5;
                } else {
                    position = position - 5;
                }
                break;
            case MenuStyleCompact:
                if((position % 16) < 8) {
                    position = position + 8;
                } else {
                    position = position - 8;
                }
                break;

            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_right(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(momentum_settings.menu_style) {
            case MenuStyleWii:
                if(count % 2) {
                    if(position == count - 1) {
                        position = 0;
                    } else if(position == count - 2) {
                        position = count - 1;
                    } else {
                        position += 2;
                    }
                } else {
                    position += 2;
                    if(position >= count) {
                        position = position % 2;
                    }
                }
                break;
            case MenuStyleDsi:
            case MenuStylePs4:
            case MenuStyleVertical:
                size_t vertical_offset = model->vertical_offset;
                if(position < count - 1) {
                    position++;
                    if(vertical_offset < count - 8 && vertical_offset == position - 7) {
                        vertical_offset++;
                    }
                } else {
                    position = 0;
                    vertical_offset = 0;
                }
                model->vertical_offset = vertical_offset;
                break;
            case MenuStyleC64:
                if((position % 10) < 5) {
                    position = position + 5;
                } else {
                    position = position - 5;
                }
                break;
            case MenuStyleCompact:
                if((position % 16) < 8) {
                    position = position + 8;
                } else {
                    position = position - 8;
                }
                break;

            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_ok(Menu* menu) {
    MenuItem* item = NULL;
    with_view_model(
        menu->view,
        MenuModel * model,
        {
            if(MenuItemArray_size(model->items)) {
                item = MenuItemArray_get(model->items, model->position);
            }
        },
        true);
    if(item && item->callback) {
        item->callback(item->callback_context, item->index);
    }
}
