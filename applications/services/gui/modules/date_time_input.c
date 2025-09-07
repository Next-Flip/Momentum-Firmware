#include "date_time_input.h"
#include <furi.h>
#include <assets_icons.h>

#define get_state(m, r, c)                                           \
    ((m)->row == (r) && (m)->column == (c) ?                         \
         ((m)->editing ? EditStateActiveEditing : EditStateActive) : \
         EditStateNone)

#define ROW_0_Y (10)
#define ROW_0_H (20)

#define ROW_1_Y (40)
#define ROW_1_H (20)

#define ROW_COUNT    2
#define COLUMN_COUNT 3

struct DateTimeInput {
    View* view;
};

typedef struct {
    const char* header;
    DateTime* datetime;

    uint8_t row;
    uint8_t column;
    bool editing;

    DateTimeChangedCallback changed_callback;
    DateTimeDoneCallback done_callback;
    void* callback_context;
} DateTimeInputModel;

typedef enum {
    EditStateNone,
    EditStateActive,
    EditStateActiveEditing,
} EditState;

static inline void date_time_input_cleanup_date(DateTime* dt) {
    uint8_t day_per_month =
        datetime_get_days_per_month(datetime_is_leap_year(dt->year), dt->month);
    if(dt->day > day_per_month) {
        dt->day = day_per_month;
    }
}
static inline void date_time_input_draw_block(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t w,
    size_t h,
    Font font,
    EditState state,
    const char* text) {
    furi_assert(canvas);
    furi_assert(text);

    canvas_set_color(canvas, ColorBlack);
    if(state != EditStateNone) {
        if(state == EditStateActiveEditing) {
            canvas_draw_icon(canvas, x + w / 2 - 2, y - 1 - 3, &I_SmallArrowUp_3x5);
            canvas_draw_icon(canvas, x + w / 2 - 2, y + h + 1, &I_SmallArrowDown_3x5);
        }
        canvas_draw_rbox(canvas, x, y, w, h, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 1);
    }

    canvas_set_font(canvas, font);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, text);
    if(state != EditStateNone) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void date_time_input_draw_time_callback(Canvas* canvas, DateTimeInputModel* model) {
    furi_check(model->datetime);

    char buffer[64];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, ROW_1_Y - 2, "                 H    H       M   M      S    S");
    canvas_set_font(canvas, FontPrimary);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime->hour);
    date_time_input_draw_block(
        canvas, 30, ROW_1_Y, 28, ROW_1_H, FontBigNumbers, get_state(model, 1, 0), buffer);
    canvas_draw_box(canvas, 60, ROW_1_Y + ROW_1_H - 7, 2, 2);
    canvas_draw_box(canvas, 60, ROW_1_Y + ROW_1_H - 7 - 6, 2, 2);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime->minute);
    date_time_input_draw_block(
        canvas, 64, ROW_1_Y, 28, ROW_1_H, FontBigNumbers, get_state(model, 1, 1), buffer);
    canvas_draw_box(canvas, 94, ROW_1_Y + ROW_1_H - 7, 2, 2);
    canvas_draw_box(canvas, 94, ROW_1_Y + ROW_1_H - 7 - 6, 2, 2);

    snprintf(buffer, sizeof(buffer), "%02u", model->datetime->second);
    date_time_input_draw_block(
        canvas, 98, ROW_1_Y, 28, ROW_1_H, FontBigNumbers, get_state(model, 1, 2), buffer);
}

static void date_time_input_draw_date_callback(Canvas* canvas, DateTimeInputModel* model) {
    furi_check(model->datetime);

    char buffer[64];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, ROW_0_Y - 2, "     Y   Y   Y   Y        M   M      D    D");
    canvas_set_font(canvas, FontPrimary);
    snprintf(buffer, sizeof(buffer), "%04u", model->datetime->year);
    date_time_input_draw_block(
        canvas, 2, ROW_0_Y, 56, ROW_0_H, FontBigNumbers, get_state(model, 0, 0), buffer);
    snprintf(buffer, sizeof(buffer), "%02u", model->datetime->month);
    date_time_input_draw_block(
        canvas, 64, ROW_0_Y, 28, ROW_0_H, FontBigNumbers, get_state(model, 0, 1), buffer);
    canvas_draw_box(canvas, 64 - 5, ROW_0_Y + (ROW_0_H / 2), 4, 2);
    snprintf(buffer, sizeof(buffer), "%02u", model->datetime->day);
    date_time_input_draw_block(
        canvas, 98, ROW_0_Y, 28, ROW_0_H, FontBigNumbers, get_state(model, 0, 2), buffer);
    canvas_draw_box(canvas, 98 - 5, ROW_0_Y + (ROW_0_H / 2), 4, 2);
}

static void date_time_input_view_draw_callback(Canvas* canvas, void* _model) {
    DateTimeInputModel* model = _model;
    canvas_clear(canvas);
    date_time_input_draw_time_callback(canvas, model);
    date_time_input_draw_date_callback(canvas, model);
}

static bool date_time_input_navigation_callback(InputEvent* event, DateTimeInputModel* model) {
    if(event->key == InputKeyUp) {
        if(model->row > 0) model->row--;
    } else if(event->key == InputKeyDown) {
        if(model->row < ROW_COUNT - 1) model->row++;
    } else if(event->key == InputKeyOk) {
        model->editing = !model->editing;
    } else if(event->key == InputKeyRight) {
        if(model->column < COLUMN_COUNT - 1) model->column++;
    } else if(event->key == InputKeyLeft) {
        if(model->column > 0) model->column--;
    } else if(event->key == InputKeyBack && model->editing) {
        model->editing = false;
    } else if(event->key == InputKeyBack && model->done_callback) {
        model->done_callback(model->callback_context);
    } else {
        return false;
    }

    return true;
}

static bool date_time_input_time_callback(InputEvent* event, DateTimeInputModel* model) {
    furi_check(model->datetime);

    if(event->key == InputKeyUp) {
        if(model->column == 0) {
            model->datetime->hour++;
            model->datetime->hour = model->datetime->hour % 24;
        } else if(model->column == 1) {
            model->datetime->minute++;
            model->datetime->minute = model->datetime->minute % 60;
        } else if(model->column == 2) {
            model->datetime->second++;
            model->datetime->second = model->datetime->second % 60;
        } else {
            furi_crash();
        }
    } else if(event->key == InputKeyDown) {
        if(model->column == 0) {
            if(model->datetime->hour > 0) {
                model->datetime->hour--;
            } else {
                model->datetime->hour = 23;
            }
            model->datetime->hour = model->datetime->hour % 24;
        } else if(model->column == 1) {
            if(model->datetime->minute > 0) {
                model->datetime->minute--;
            } else {
                model->datetime->minute = 59;
            }
            model->datetime->minute = model->datetime->minute % 60;
        } else if(model->column == 2) {
            if(model->datetime->second > 0) {
                model->datetime->second--;
            } else {
                model->datetime->second = 59;
            }
            model->datetime->second = model->datetime->second % 60;
        } else {
            furi_crash();
        }
    } else {
        return date_time_input_navigation_callback(event, model);
    }

    return true;
}

static bool date_time_input_date_callback(InputEvent* event, DateTimeInputModel* model) {
    furi_check(model->datetime);

    if(event->key == InputKeyUp) {
        if(model->column == 0) {
            if(model->datetime->year < 2099) {
                model->datetime->year++;
            }
        } else if(model->column == 1) {
            if(model->datetime->month < 12) {
                model->datetime->month++;
            }
        } else if(model->column == 2) {
            if(model->datetime->day < 31) model->datetime->day++;
        } else {
            furi_crash();
        }
    } else if(event->key == InputKeyDown) {
        if(model->column == 0) {
            if(model->datetime->year > 1980) {
                model->datetime->year--;
            }
        } else if(model->column == 1) {
            if(model->datetime->month > 1) {
                model->datetime->month--;
            }
        } else if(model->column == 2) {
            if(model->datetime->day > 1) {
                model->datetime->day--;
            }
        } else {
            furi_crash();
        }
    } else {
        return date_time_input_navigation_callback(event, model);
    }

    date_time_input_cleanup_date(model->datetime);

    return true;
}

static bool date_time_input_view_input_callback(InputEvent* event, void* context) {
    DateTimeInput* instance = context;
    bool consumed = false;

    with_view_model(
        instance->view,
        DateTimeInputModel * model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(model->editing) {
                    if(model->row == 0) {
                        consumed = date_time_input_date_callback(event, model);
                    } else if(model->row == 1) {
                        consumed = date_time_input_time_callback(event, model);
                    } else {
                        furi_crash();
                    }

                    if(model->changed_callback) {
                        model->changed_callback(model->callback_context);
                    }
                } else {
                    consumed = date_time_input_navigation_callback(event, model);
                }
            }
        },
        true);

    return consumed;
}

/** Reset all input-related data in model
 *
 * @param      model  The model
 */
static void date_time_input_reset_model_input_data(DateTimeInputModel* model) {
    model->row = 0;
    model->column = 0;

    model->datetime = NULL;
}

DateTimeInput* date_time_input_alloc(void) {
    DateTimeInput* date_time_input = malloc(sizeof(DateTimeInput));
    date_time_input->view = view_alloc();
    view_allocate_model(date_time_input->view, ViewModelTypeLocking, sizeof(DateTimeInputModel));
    view_set_context(date_time_input->view, date_time_input);
    view_set_draw_callback(date_time_input->view, date_time_input_view_draw_callback);
    view_set_input_callback(date_time_input->view, date_time_input_view_input_callback);

    with_view_model(
        date_time_input->view,
        DateTimeInputModel * model,
        {
            model->header = "";
            model->changed_callback = NULL;
            model->callback_context = NULL;
            date_time_input_reset_model_input_data(model);
        },
        true);

    return date_time_input;
}

void date_time_input_free(DateTimeInput* date_time_input) {
    furi_check(date_time_input);
    view_free(date_time_input->view);
    free(date_time_input);
}

View* date_time_input_get_view(DateTimeInput* date_time_input) {
    furi_check(date_time_input);
    return date_time_input->view;
}

void date_time_input_set_result_callback(
    DateTimeInput* date_time_input,
    DateTimeChangedCallback changed_callback,
    DateTimeDoneCallback done_callback,
    void* callback_context,
    DateTime* current_datetime) {
    furi_check(date_time_input);

    with_view_model(
        date_time_input->view,
        DateTimeInputModel * model,
        {
            date_time_input_reset_model_input_data(model);
            model->changed_callback = changed_callback;
            model->done_callback = done_callback;
            model->callback_context = callback_context;
            model->datetime = current_datetime;
        },
        true);
}

void date_time_input_set_header_text(DateTimeInput* date_time_input, const char* text) {
    furi_check(date_time_input);

    with_view_model(
        date_time_input->view, DateTimeInputModel * model, { model->header = text; }, true);
}
