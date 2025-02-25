#include "assets_icons.h"
#include "path.h"
#include "widget_element_i.h"
#include <gui/elements.h>
#include <core/common_defines.h>
#include "archive/archive_i.h"
#include "assets_icons.h"

#define SCROLL_INTERVAL (333)
#define SCROLL_DELAY    (2)

const char* units_short[] = {"B", "K", "M", "G", "T"};

typedef struct {
    FuriString* path;
    const Icon* icon;
    char size_num[8];
    char size_unit[2];
} FileListItem;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t lines;
    FileListItem* files;
    size_t count;
    size_t offset;
    uint8_t scrollbar_y;
    bool show_size;
    uint8_t scrollbar_height;
    size_t scroll_counter;
    FuriTimer* scroll_timer;
} FileListModel;

static void format_file_size(
    uint64_t size,
    char* num_buf,
    size_t num_size,
    char* unit_buf,
    size_t unit_size) {
    double formatted_size = size;
    uint8_t unit = 0;

    while(formatted_size >= 1024 && unit < COUNT_OF(units_short) - 1) {
        formatted_size /= 1024;
        unit++;
    }

    if(unit == 0) {
        snprintf(num_buf, num_size, "%d", (int)formatted_size);
    } else {
        snprintf(num_buf, num_size, "%.1f", (double)formatted_size);
    }
    snprintf(unit_buf, unit_size, "%s", units_short[unit]);
}

static void widget_element_file_list_draw(Canvas* canvas, WidgetElement* element) {
    furi_assert(canvas);
    furi_assert(element);
    FileListModel* model = element->model;
    size_t items_visible = MIN(model->lines, model->count);

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < items_visible; i++) {
        size_t idx = model->offset + i;
        if(idx < model->count) {
            canvas_draw_icon(
                canvas, model->x + 2, model->y + (i * FRAME_HEIGHT) - 9, model->files[idx].icon);

            size_t inner_x = 123;
            if(model->show_size && model->files[idx].size_num[0] != '\0') {
                canvas_set_font(canvas, FontPrimary);
                uint16_t num_width = canvas_string_width(canvas, model->files[idx].size_num);
                canvas_set_font(canvas, FontSecondary);
                uint16_t unit_width = canvas_string_width(canvas, model->files[idx].size_unit);
                uint16_t total_width = num_width + unit_width;
                inner_x = model->x + (128 - model->x) - total_width - 5;
                inner_x--;

                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str(
                    canvas, inner_x, model->y + (i * FRAME_HEIGHT), model->files[idx].size_num);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(
                    canvas,
                    inner_x + num_width + 1,
                    model->y + (i * FRAME_HEIGHT),
                    model->files[idx].size_unit);
            }

            size_t scroll_counter = model->scroll_counter;
            scroll_counter =
                i == 0 && model->count > model->lines ?
                    (scroll_counter < SCROLL_DELAY ? 0 : scroll_counter - SCROLL_DELAY) :
                    0;

            elements_scrollable_text_line(
                canvas,
                model->x + 15,
                model->y + (i * FRAME_HEIGHT),
                inner_x - 19,
                model->files[idx].path,
                scroll_counter,
                i != 0 || model->count <= model->lines);
        }
    }

    if(model->count > model->lines) {
        elements_scrollbar_pos(
            canvas,
            128,
            model->scrollbar_y,
            model->scrollbar_height,
            model->offset,
            model->count - (model->lines - 1));
    }
}

static bool widget_element_file_list_input(InputEvent* event, WidgetElement* element) {
    furi_assert(element);
    FileListModel* model = element->model;
    bool consumed = false;

    if(model->count > model->lines &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(event->key == InputKeyUp) {
            model->offset = (model->offset > 0) ? model->offset - 1 : model->count - model->lines;
            model->scroll_counter = 0;
            consumed = true;
        } else if(event->key == InputKeyDown) {
            model->offset = ((model->offset + model->lines) < model->count) ? model->offset + 1 :
                                                                              0;
            model->scroll_counter = 0;
            consumed = true;
        }
    }

    return consumed;
}

static void widget_element_file_list_timer_callback(void* context) {
    WidgetElement* element = context;
    FileListModel* file_model = element->model;
    file_model->scroll_counter++;
    with_view_model(element->parent->view, void* _model, { UNUSED(_model); }, true);
}

static void widget_element_file_list_free(WidgetElement* element) {
    furi_assert(element);
    FileListModel* model = element->model;
    furi_timer_stop(model->scroll_timer);
    furi_timer_free(model->scroll_timer);
    for(size_t i = 0; i < model->count; i++) {
        furi_string_free(model->files[i].path);
    }
    free(model->files);
    free(model);
    free(element);
}

WidgetElement* widget_element_file_list_create(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t lines,
    FuriString** files,
    size_t count,
    uint8_t scrollbar_y,
    uint8_t scrollbar_height,
    bool show_size) {
    // Allocate and init model
    FileListModel* model = malloc(sizeof(FileListModel));
    model->x = x;
    model->y = y;
    model->lines = lines;
    model->count = count;
    model->scrollbar_y = scrollbar_y;
    model->scrollbar_height = scrollbar_height;
    model->show_size = show_size;
    model->offset = 0;
    model->scroll_counter = 0;
    model->files = malloc(sizeof(FileListItem) * count);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    for(size_t i = 0; i < count; i++) {
        model->files[i].path = furi_string_alloc();
        path_extract_filename(files[i], model->files[i].path, false);
        model->files[i].size_num[0] = '\0';
        model->files[i].size_unit[0] = '\0';

        if(storage_dir_exists(storage, furi_string_get_cstr(files[i]))) {
            model->files[i].icon = &I_dir_10px;
        } else {
            const char* ext = strrchr(furi_string_get_cstr(model->files[i].path), '.');
            model->files[i].icon = (ext && strcasecmp(ext, ".js") == 0) ? &I_js_script_10px :
                                                                          &I_unknown_10px;
        }

        if(show_size) {
            storage_common_stat(storage, furi_string_get_cstr(files[i]), &info);
            format_file_size(
                info.size,
                model->files[i].size_num,
                sizeof(model->files[i].size_num),
                model->files[i].size_unit,
                sizeof(model->files[i].size_unit));
        }
    }
    furi_record_close(RECORD_STORAGE);

    // Allocate and init Element
    WidgetElement* element = malloc(sizeof(WidgetElement));
    element->draw = widget_element_file_list_draw;
    element->input = widget_element_file_list_input;
    element->free = widget_element_file_list_free;
    element->parent = widget;
    element->model = model;

    model->scroll_timer =
        furi_timer_alloc(widget_element_file_list_timer_callback, FuriTimerTypePeriodic, element);
    furi_timer_start(model->scroll_timer, SCROLL_INTERVAL);

    return element;
}
