/**
 * @file kiisu_light_ads.c
 * @brief Light level ADC.
  Based on ADC example.
 */
#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>

#include <lib/u8g2/u8g2.h>
#include <lib/u8g2/u8g2_fonts.c>

#define FONT_HEIGHT (8u)

typedef float (*ValueConverter)(FuriHalAdcHandle* handle, uint16_t value);

typedef struct {
    const GpioPinRecord* pin;
    float value;
    ValueConverter converter;
    const char* suffix;
} DataItem;

typedef struct {
    size_t count;
    DataItem* items;
} Data;

const GpioPinRecord item_light = {.name = "LGHT", .channel = FuriHalAdcChannel4};
const GpioPinRecord item_vref  = {.name = "VREF", .channel = FuriHalAdcChannelVREFINT};
const GpioPinRecord item_temp  = {.name = "TEMP", .channel = FuriHalAdcChannelTEMPSENSOR};
const GpioPinRecord item_vbat  = {.name = "VBAT", .channel = FuriHalAdcChannelVBAT};

static void app_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    Data* data = ctx;

    canvas_set_custom_u8g2_font(canvas, u8g_font_5x8); // lib/u8g2/u8g2_fonts.c
     
    char buffer[64];
    int32_t x = 0, y = FONT_HEIGHT;
    for(size_t i = 0; i < data->count; i++) {
        if(i == canvas_height(canvas) / FONT_HEIGHT) {
            x = 64;
            y = FONT_HEIGHT;
        }

        snprintf(
            buffer,
            sizeof(buffer),
            "%4s: %4.0f%s\n",
            data->items[i].pin->name,
            (double)data->items[i].value,
            data->items[i].suffix);
        canvas_draw_str(canvas, x, y, buffer);
        y += FONT_HEIGHT;

        if(i == 0) { //Light level
            char line[32];            
            snprintf(line, sizeof(line), "%4.0f%%", 100 - ((double)data->items[i].value / 1187 * 100));
            canvas_draw_str(canvas, x, y, line);
            y += FONT_HEIGHT;
        }
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t kiisu_light_adc_main(void* p) {
    UNUSED(p);

    // Data
    Data data = {};
    data.count += 4; // Special channels
    data.items = malloc(data.count * sizeof(DataItem));
    size_t item_pos = 0;
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullDown, GpioSpeedHigh);
    data.items[item_pos].pin = &item_light;
    data.items[item_pos].converter = furi_hal_adc_convert_to_voltage;
    data.items[item_pos].suffix = "mV / 1187mV";
    item_pos++;
    data.items[item_pos].pin = &item_temp;
    data.items[item_pos].converter = furi_hal_adc_convert_temp;
    data.items[item_pos].suffix = "C";
    item_pos++;
    data.items[item_pos].pin = &item_vref;
    data.items[item_pos].converter = furi_hal_adc_convert_vref;
    data.items[item_pos].suffix = "mV";
    item_pos++;
    data.items[item_pos].pin = &item_vbat;
    data.items[item_pos].converter = furi_hal_adc_convert_vbat;
    data.items[item_pos].suffix = "mV";
    item_pos++;
    furi_assert(item_pos == data.count);

    // Alloc message queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, &data);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Initialize ADC
    FuriHalAdcHandle* adc_handle = furi_hal_adc_acquire();
    furi_hal_adc_configure(adc_handle);

    // Process events
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress && event.key == InputKeyBack) {
                running = false;
            }
        } else {
            for(size_t i = 0; i < data.count; i++) {
                data.items[i].value = data.items[i].converter(
                    adc_handle, furi_hal_adc_read(adc_handle, data.items[i].pin->channel));
            }
            view_port_update(view_port);
        }
    }

    furi_hal_adc_release(adc_handle);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(data.items);

    return 0;
}
