#include "variable_item_list.h"
#include <gui/elements.h>
#include <gui/canvas.h>
#include <furi.h>
#include <assets_icons.h>
#include <m-array.h>
#include <stdint.h>

struct VariableItem {
    FuriString* label;
    uint8_t current_value_index;
    FuriString* current_value_text;
    uint8_t values_count;
    VariableItemChangeCallback change_callback;
    void* context;

    bool locked;
    FuriString* locked_message;
};

ARRAY_DEF(VariableItemArray, VariableItem, M_POD_OPLIST);

struct VariableItemList {
    View* view;
    VariableItemListEnterCallback callback;
    void* context;

    FuriTimer* scroll_timer;
    FuriTimer* locked_timer;
};

typedef struct {
    VariableItem* item;
    uint8_t current_page;
    uint8_t selected_option_index;
    uint8_t total_options;
    uint8_t total_pages;
} ZapperMenu;

typedef struct {
    VariableItemArray_t items;
    uint8_t position;
    uint8_t window_position;

    FuriString* header;
    size_t scroll_counter;
    bool locked_message_visible;

    /// zapper menu
    bool is_zapper_menu_active;
    ZapperMenu zapper_menu;
} VariableItemListModel;

static const char* allow_zapper_field[] = {"Frequency", "Modulation"};

static void variable_item_list_process_up(VariableItemList* variable_item_list);
static void variable_item_list_process_down(VariableItemList* variable_item_list);
static void variable_item_list_process_left(VariableItemList* variable_item_list);
static void variable_item_list_process_right(VariableItemList* variable_item_list);
static void variable_item_list_process_ok(VariableItemList* variable_item_list);
static void variable_item_list_process_ok_long(VariableItemList* variable_item_list);

static size_t variable_item_list_items_on_screen(VariableItemListModel* model) {
    size_t res = 4;
    return (furi_string_empty(model->header)) ? res : res - 1;
}

static void zapper_menu_draw(Canvas* canvas, VariableItemListModel* model) {
    ZapperMenu* zapper_menu = &model->zapper_menu;
    VariableItem* item = zapper_menu->item;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    uint8_t start_option = zapper_menu->current_page * 4;
    uint8_t end_option = start_option + 4;
    if(end_option > zapper_menu->total_options) {
        end_option = zapper_menu->total_options;
    }

    uint8_t original_index = item->current_value_index;

    for(uint8_t i = start_option; i < end_option; i++) {
        uint8_t item_position = i - start_option;

        item->current_value_index = i;
        if(item->change_callback) {
            item->change_callback(item);
        }
        const char* option_text = furi_string_get_cstr(item->current_value_text);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 4, item_position * 14 + 9 + (item_position + 1) * 1, option_text);
    }

    // reset current_value_index
    item->current_value_index = original_index;
    if(item->change_callback) {
        item->change_callback(item);
    }

//scroll bar
#define SCROLL_BAR_HEIGHT 0
    const uint8_t scroll_bar_y = canvas_height(canvas) - SCROLL_BAR_HEIGHT;
    elements_scrollbar_horizontal(
        canvas,
        0,
        scroll_bar_y,
        canvas_width(canvas),
        zapper_menu->current_page,
        zapper_menu->total_pages);

//frame
#define GAP_SIZE_PX  1
#define FRAME_HEIGHT 14
    for(int i = 0; i < 4; i++) {
        uint8_t y = i * (FRAME_HEIGHT + GAP_SIZE_PX);
        canvas_draw_rframe(canvas, 0, y, canvas_width(canvas), FRAME_HEIGHT, 3);
    }

//arrow
#define ARROR_SIZE 8
    const uint8_t arrow_x = canvas_width(canvas) - 9;
    // ^
    canvas_draw_triangle(
        canvas, arrow_x, 16 - (16 - 9) / 2 - 3, ARROR_SIZE, ARROR_SIZE, CanvasDirectionBottomToTop);
    // <
    canvas_draw_triangle(
        canvas, arrow_x + 9 / 2, 24 - 2, ARROR_SIZE, ARROR_SIZE, CanvasDirectionRightToLeft);
    // >
    canvas_draw_triangle(
        canvas, arrow_x - 9 / 2 + 2, 40 - 4, ARROR_SIZE, ARROR_SIZE, CanvasDirectionLeftToRight);
    // v
    canvas_draw_triangle(
        canvas, arrow_x, 16 * 3 + 6 - 6, ARROR_SIZE, ARROR_SIZE, CanvasDirectionTopToBottom);
}

static void variable_item_list_draw_callback(Canvas* canvas, void* _model) {
    VariableItemListModel* model = _model;
    canvas_clear(canvas);

    if(model->is_zapper_menu_active) {
        // paint
        zapper_menu_draw(canvas, model);
        return;
    }

    const uint8_t item_height = 16;
    uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if(!furi_string_empty(model->header)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 11, furi_string_get_cstr(model->header));
    }

    uint8_t position = 0;
    VariableItemArray_it_t it;

    canvas_set_font(canvas, FontSecondary);
    for(VariableItemArray_it(it, model->items); !VariableItemArray_end_p(it);
        VariableItemArray_next(it)) {
        uint8_t item_position = position - model->window_position;
        uint8_t items_on_screen = variable_item_list_items_on_screen(model);
        uint8_t y_offset = furi_string_empty(model->header) ? 0 : item_height;

        if(item_position < items_on_screen) {
            const VariableItem* item = VariableItemArray_cref(it);
            uint8_t item_y = y_offset + (item_position * item_height);
            uint8_t item_text_y = item_y + item_height - 4;
            size_t scroll_counter = 0;

            if(position == model->position) {
                canvas_set_color(canvas, ColorBlack);
                elements_slightly_rounded_box(canvas, 0, item_y + 1, item_width, item_height - 2);
                canvas_set_color(canvas, ColorWhite);
                scroll_counter = model->scroll_counter;
                if(scroll_counter < 1) { // Show text beginning a little longer
                    scroll_counter = 0;
                } else {
                    scroll_counter -= 1;
                }
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            uint8_t value_pos_x = 73;
            uint8_t label_width = 66;
            if(item->locked) {
                // Span label up to lock icon
                value_pos_x = 110;
                label_width = 100;
            } else if(item->current_value_index == 0 && furi_string_empty(item->current_value_text)) {
                // Only label text, no value text, show longer label
                label_width = 109;
            } else if(furi_string_size(item->current_value_text) < 4U) {
                // Smaller value section for short values
                value_pos_x = 80;
                label_width = 71;
            }

            elements_scrollable_text_line(
                canvas,
                6,
                item_text_y,
                label_width,
                item->label,
                scroll_counter,
                (position != model->position));

            if(item->locked) {
                canvas_draw_icon(canvas, value_pos_x, item_text_y - 8, &I_Lock_7x8);
            } else {
                if(item->current_value_index > 0) {
                    canvas_draw_str(canvas, value_pos_x, item_text_y, "<");
                }

                elements_scrollable_text_line_centered(
                    canvas,
                    (115 + value_pos_x) / 2 + 1,
                    item_text_y,
                    37,
                    item->current_value_text,
                    scroll_counter,
                    false,
                    true);

                if(item->current_value_index < (item->values_count - 1)) {
                    canvas_draw_str(canvas, 115, item_text_y, ">");
                }
            }
        }

        position++;
    }

    elements_scrollbar(canvas, model->position, VariableItemArray_size(model->items));

    if(model->locked_message_visible) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 8, 10, 110, 48);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, 10, 14, &I_WarningDolphin_45x42);
        canvas_draw_rframe(canvas, 8, 8, 112, 50, 3);
        canvas_draw_rframe(canvas, 9, 9, 110, 48, 2);
        elements_multiline_text_aligned(
            canvas,
            84,
            32,
            AlignCenter,
            AlignCenter,
            furi_string_get_cstr(
                VariableItemArray_get(model->items, model->position)->locked_message));
    }
}

void variable_item_list_set_selected_item(VariableItemList* variable_item_list, uint8_t index) {
    furi_check(variable_item_list);
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            uint8_t position = index;
            const size_t items_count = VariableItemArray_size(model->items);
            uint8_t items_on_screen = variable_item_list_items_on_screen(model);

            if(position >= items_count) {
                position = 0;
            }

            model->position = position;
            model->window_position = position;

            if(model->window_position > 0) {
                model->window_position -= 1;
            }

            if(items_count <= items_on_screen) {
                model->window_position = 0;
            } else {
                const size_t pos = items_count - items_on_screen;
                if(model->window_position > pos) {
                    model->window_position = pos;
                }
            }
        },
        true);
}

uint8_t variable_item_list_get_selected_item_index(VariableItemList* variable_item_list) {
    furi_check(variable_item_list);
    VariableItemListModel* model = view_get_model(variable_item_list->view);
    uint8_t idx = model->position;
    view_commit_model(variable_item_list->view, false);
    return idx;
}

void variable_item_list_set_header(VariableItemList* variable_item_list, const char* header) {
    furi_check(variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            if(header == NULL) {
                furi_string_reset(model->header);
            } else {
                furi_string_set_str(model->header, header);
            }
        },
        true);
}

static bool zapper_menu_input_handler(InputEvent* event, void* context) {
    VariableItemList* variable_item_list = context;
    bool consumed = true;
    // this ok_ever_released var is for: prevent to trigger shuffing when first enter, because without that, would make muscle memory press not possible,
    // because it would start shuffle when long pressed OK and entered zapper menu if user didn't release OK in time (consumed system seems didn't consider this kind of edge case).
    // the static usage is because make it keeps life among func calls,
    // otherwise it would be reseted to false each time enter this func, but each calls would make it false.
    // it now seted back to false when exit zapper menu.
    static bool ok_ever_released = false;

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            ZapperMenu* zapper_menu = &model->zapper_menu;
            VariableItem* item = zapper_menu->item;

            if(event->type == InputTypeRelease && event->key == InputKeyOk) {
                ok_ever_released = true;
            } else if(event->type == InputTypeShort) {
                uint8_t selected_option = 0xFF; // as nullptr
                switch(event->key) {
                case InputKeyUp:
                    selected_option = zapper_menu->current_page * 4 + 0;
                    break;
                case InputKeyLeft:
                    selected_option = zapper_menu->current_page * 4 + 1;
                    break;
                case InputKeyRight:
                    selected_option = zapper_menu->current_page * 4 + 2;
                    break;
                case InputKeyDown:
                    selected_option = zapper_menu->current_page * 4 + 3;
                    break;
                case InputKeyOk:
                    // paging
                    zapper_menu->current_page =
                        (zapper_menu->current_page + 1) % zapper_menu->total_pages;
                    // reset sel-ed one
                    zapper_menu->selected_option_index = 0;
                    break;
                case InputKeyBack:
                    // exit
                    ok_ever_released = false;
                    model->is_zapper_menu_active = false;
                    break;
                default:
                    break;
                }

                // check valid
                if(selected_option != 0xFF &&
                   selected_option < zapper_menu->total_options) { //0xFF use as nullptr
                    // update anf callback
                    item->current_value_index = selected_option;
                    if(item->change_callback) {
                        item->change_callback(item);
                    }
                    // exit
                    ok_ever_released = false;
                    model->is_zapper_menu_active = false;
                }
            } else if(event->type == InputTypeRepeat) {
                if(event->key == InputKeyOk && ok_ever_released) {
                    zapper_menu->current_page =
                        (zapper_menu->current_page + 1) % zapper_menu->total_pages;
                } else if(event->key == InputKeyBack) {
                    ok_ever_released = false;
                    model->is_zapper_menu_active = false;
                }
            } else if(event->type == InputTypeLong && event->key == InputKeyBack) {
                ok_ever_released = false;
                model->is_zapper_menu_active = false;
            }
        },
        true);

    return consumed;
}

static bool variable_item_list_input_callback(InputEvent* event, void* context) {
    VariableItemList* variable_item_list = context;
    furi_assert(variable_item_list);
    bool consumed = false;

    bool locked_message_visible = false;
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            if(model->is_zapper_menu_active) {
                consumed = zapper_menu_input_handler(event, context);
            } else {
                locked_message_visible = model->locked_message_visible;
            }
        },
        false);

    if(consumed) return true;

    if((event->type != InputTypePress && event->type != InputTypeRelease) &&
       locked_message_visible) {
        with_view_model(
            variable_item_list->view,
            VariableItemListModel * model,
            { model->locked_message_visible = false; },
            true);
        consumed = true;
    } else if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            variable_item_list_process_up(variable_item_list);
            break;
        case InputKeyDown:
            consumed = true;
            variable_item_list_process_down(variable_item_list);
            break;
        case InputKeyLeft:
            consumed = true;
            variable_item_list_process_left(variable_item_list);
            break;
        case InputKeyRight:
            consumed = true;
            variable_item_list_process_right(variable_item_list);
            break;
        case InputKeyOk:
            variable_item_list_process_ok(variable_item_list);
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            variable_item_list_process_up(variable_item_list);
            break;
        case InputKeyDown:
            consumed = true;
            variable_item_list_process_down(variable_item_list);
            break;
        case InputKeyLeft:
            consumed = true;
            variable_item_list_process_left(variable_item_list);
            break;
        case InputKeyRight:
            consumed = true;
            variable_item_list_process_right(variable_item_list);
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeLong) {
        switch(event->key) {
        case InputKeyOk:
            consumed = true;
            variable_item_list_process_ok_long(variable_item_list);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void variable_item_list_process_up(VariableItemList* variable_item_list) {
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            uint8_t items_on_screen = variable_item_list_items_on_screen(model);
            if(model->position > 0) {
                model->position--;

                if((model->position == model->window_position) && (model->window_position > 0)) {
                    model->window_position--;
                }
            } else {
                model->position = VariableItemArray_size(model->items) - 1;
                if(model->position > (items_on_screen - 1)) {
                    model->window_position = model->position - (items_on_screen - 1);
                }
            }
            model->scroll_counter = 0;
        },
        true);
}

void variable_item_list_process_down(VariableItemList* variable_item_list) {
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            uint8_t items_on_screen = variable_item_list_items_on_screen(model);
            if(model->position < (VariableItemArray_size(model->items) - 1)) {
                model->position++;
                if((model->position - model->window_position) > (items_on_screen - 2) &&
                   model->window_position <
                       (VariableItemArray_size(model->items) - items_on_screen)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
            model->scroll_counter = 0;
        },
        true);
}

VariableItem* variable_item_list_get_selected_item(VariableItemListModel* model) {
    VariableItem* item = NULL;

    VariableItemArray_it_t it;
    uint8_t position = 0;
    for(VariableItemArray_it(it, model->items); !VariableItemArray_end_p(it);
        VariableItemArray_next(it)) {
        if(position == model->position) {
            break;
        }
        position++;
    }

    item = VariableItemArray_ref(it);

    furi_assert(item);
    return item;
}

void variable_item_list_process_left(VariableItemList* variable_item_list) {
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItem* item = variable_item_list_get_selected_item(model);
            if(item->locked) {
                model->locked_message_visible = true;
                furi_timer_start(
                    variable_item_list->locked_timer, furi_kernel_get_tick_frequency() * 3);
            } else if(item->current_value_index > 0) {
                item->current_value_index--;
                model->scroll_counter = 0;
                if(item->change_callback) {
                    item->change_callback(item);
                }
            }
        },
        true);
}

void variable_item_list_process_right(VariableItemList* variable_item_list) {
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItem* item = variable_item_list_get_selected_item(model);
            if(item->locked) {
                model->locked_message_visible = true;
                furi_timer_start(
                    variable_item_list->locked_timer, furi_kernel_get_tick_frequency() * 3);
            } else if(item->current_value_index < (item->values_count - 1)) {
                item->current_value_index++;
                model->scroll_counter = 0;
                if(item->change_callback) {
                    item->change_callback(item);
                }
            }
        },
        true);
}

void variable_item_list_process_ok(VariableItemList* variable_item_list) {
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItem* item = variable_item_list_get_selected_item(model);
            if(item->locked) {
                model->locked_message_visible = true;
                furi_timer_start(
                    variable_item_list->locked_timer, furi_kernel_get_tick_frequency() * 3);
            } else if(variable_item_list->callback) {
                variable_item_list->callback(variable_item_list->context, model->position);
            }
        },
        true);
}

void variable_item_list_process_ok_long(VariableItemList* variable_item_list) {
    furi_check(variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItem* item = variable_item_list_get_selected_item(model);

            bool is_allowed = false;
            for(size_t i = 0; i < sizeof(allow_zapper_field) / sizeof(allow_zapper_field[0]);
                i++) {
                if(strcmp(furi_string_get_cstr(item->label), allow_zapper_field[i]) == 0) {
                    is_allowed = true;
                    break;
                }
            }

            if(is_allowed) {
                // init
                model->is_zapper_menu_active = true;
                ZapperMenu* zapper_menu = &model->zapper_menu;

                zapper_menu->item = item;
                zapper_menu->current_page = 0;
                zapper_menu->selected_option_index = 0;
                zapper_menu->total_options = item->values_count;
                zapper_menu->total_pages = (zapper_menu->total_options + 3) / 4;

                // update
                model->scroll_counter = 0;
            }
        },
        true);
}

static void variable_item_list_scroll_timer_callback(void* context) {
    furi_assert(context);
    VariableItemList* variable_item_list = context;
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        { model->scroll_counter++; },
        true);
}

void variable_item_list_locked_timer_callback(void* context) {
    furi_assert(context);
    VariableItemList* variable_item_list = context;

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        { model->locked_message_visible = false; },
        true);
}

VariableItemList* variable_item_list_alloc(void) {
    VariableItemList* variable_item_list = malloc(sizeof(VariableItemList));
    variable_item_list->view = view_alloc();
    view_set_context(variable_item_list->view, variable_item_list);
    view_allocate_model(
        variable_item_list->view, ViewModelTypeLocking, sizeof(VariableItemListModel));
    view_set_draw_callback(variable_item_list->view, variable_item_list_draw_callback);
    view_set_input_callback(variable_item_list->view, variable_item_list_input_callback);

    variable_item_list->locked_timer = furi_timer_alloc(
        variable_item_list_locked_timer_callback, FuriTimerTypeOnce, variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItemArray_init(model->items);
            model->position = 0;
            model->window_position = 0;
            model->header = furi_string_alloc();
            model->scroll_counter = 0;
        },
        true);
    variable_item_list->scroll_timer = furi_timer_alloc(
        variable_item_list_scroll_timer_callback, FuriTimerTypePeriodic, variable_item_list);
    furi_timer_start(variable_item_list->scroll_timer, 333);

    return variable_item_list;
}

void variable_item_list_free(VariableItemList* variable_item_list) {
    furi_check(variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            furi_string_free(model->header);
            VariableItemArray_it_t it;
            for(VariableItemArray_it(it, model->items); !VariableItemArray_end_p(it);
                VariableItemArray_next(it)) {
                furi_string_free(VariableItemArray_ref(it)->label);
                furi_string_free(VariableItemArray_ref(it)->current_value_text);
                furi_string_free(VariableItemArray_ref(it)->locked_message);
            }
            VariableItemArray_clear(model->items);
        },
        false);
    furi_timer_stop(variable_item_list->scroll_timer);
    furi_timer_free(variable_item_list->scroll_timer);
    furi_timer_stop(variable_item_list->locked_timer);
    furi_timer_free(variable_item_list->locked_timer);
    view_free(variable_item_list->view);
    free(variable_item_list);
}

void variable_item_list_reset(VariableItemList* variable_item_list) {
    furi_check(variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            VariableItemArray_it_t it;
            for(VariableItemArray_it(it, model->items); !VariableItemArray_end_p(it);
                VariableItemArray_next(it)) {
                furi_string_free(VariableItemArray_ref(it)->label);
                furi_string_free(VariableItemArray_ref(it)->current_value_text);
                furi_string_free(VariableItemArray_ref(it)->locked_message);
            }
            VariableItemArray_reset(model->items);
            furi_string_reset(model->header);
        },
        false);
}

View* variable_item_list_get_view(VariableItemList* variable_item_list) {
    furi_check(variable_item_list);
    return variable_item_list->view;
}

VariableItem* variable_item_list_add(
    VariableItemList* variable_item_list,
    const char* label,
    uint8_t values_count,
    VariableItemChangeCallback change_callback,
    void* context) {
    VariableItem* item = NULL;
    furi_check(label);
    furi_check(variable_item_list);

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            item = VariableItemArray_push_new(model->items);
            item->label = furi_string_alloc_set(label);
            item->values_count = values_count;
            item->change_callback = change_callback;
            item->context = context;
            item->current_value_index = 0;
            item->current_value_text = furi_string_alloc();
            item->locked = false;
            item->locked_message = furi_string_alloc();
        },
        true);

    return item;
}

VariableItem* variable_item_list_get(VariableItemList* variable_item_list, uint8_t position) {
    furi_check(variable_item_list);
    VariableItem* item = NULL;

    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            if(position < VariableItemArray_size(model->items)) {
                item = VariableItemArray_get(model->items, position);
            }
        },
        true);

    return item;
}

void variable_item_list_set_enter_callback(
    VariableItemList* variable_item_list,
    VariableItemListEnterCallback callback,
    void* context) {
    furi_check(callback);
    with_view_model(
        variable_item_list->view,
        VariableItemListModel * model,
        {
            UNUSED(model);
            variable_item_list->callback = callback;
            variable_item_list->context = context;
        },
        false);
}

void variable_item_set_current_value_index(VariableItem* item, uint8_t current_value_index) {
    furi_check(item);
    item->current_value_index = current_value_index;
}

void variable_item_set_values_count(VariableItem* item, uint8_t values_count) {
    furi_check(item);
    item->values_count = values_count;
}

void variable_item_set_item_label(VariableItem* item, const char* label) {
    furi_check(item);
    furi_check(label);
    furi_string_set(item->label, label);
}

void variable_item_set_current_value_text(VariableItem* item, const char* current_value_text) {
    furi_check(item);
    furi_string_set(item->current_value_text, current_value_text);
}

void variable_item_set_locked(VariableItem* item, bool locked, const char* locked_message) {
    furi_check(item);
    item->locked = locked;
    if(locked_message) {
        furi_string_set(item->locked_message, locked_message);
    } else if(locked && furi_string_empty(item->locked_message)) {
        furi_string_set(item->locked_message, "Locked!");
    }
}

uint8_t variable_item_get_current_value_index(VariableItem* item) {
    furi_check(item);
    return item->current_value_index;
}

void* variable_item_get_context(VariableItem* item) {
    furi_check(item);
    return item->context;
}
