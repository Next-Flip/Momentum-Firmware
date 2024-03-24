#include "rpc_rgb.h"

#include "colors.h"
#include "momentum.h"
#include "drivers/rgb_backlight.h"

static const RgbColor rainbow_colors[] = {
    {{255, 0, 0}}, // Red
    {{255, 130, 0}}, // Orange
    {{255, 255, 0}}, // Yellow
    {{0, 255, 0}}, // Green
    {{0, 0, 255}}, // Blue
    {{123, 0, 255}}, // Indigo
    {{255, 0, 255}} // Violet
};

typedef struct {
    uint8_t color;
    uint8_t step;
} rgb_trans;

static struct {
    rgb_trans fg;
    rgb_trans bg;
} state = {
    .fg = {0, 0},
    .bg = {0, 0},
};

RgbColor rainbow_step(rgb_trans* rgb) {
    if(rgb->step >= 20) {
        rgb->color = (rgb->color + 1) % 7;
        rgb->step = 0;
    }

    RgbColor color = interpolate_color(
        &rainbow_colors[rgb->color], &rainbow_colors[(rgb->color + 1) % 7], rgb->step, 20);
    rgb->step++;

    return color;
}

RgbColor get_screen_color_fg() {
    RgbColor color = {{0, 0, 0}};

    switch(momentum_settings.vgm_color_mode) {
    case VgmColorModeCustom:
        color = momentum_settings.vgm_color_fg;
        break;
    case VgmColorModeRainbow:
        //color = rainbow_step(&state.fg);
        break;
    case VgmColorModeRgbBacklight:
        //rgb_backlight_get_color(0, &color);
        break;
    default:
        break;
    }

    return color;
}

RgbColor get_screen_color_bg() {
    RgbColor color = {{255, 130, 0}};
    switch(momentum_settings.vgm_color_mode) {
    case VgmColorModeCustom:
        color = momentum_settings.vgm_color_bg;
        break;
    case VgmColorModeRainbow:
        color = rainbow_step(&state.bg);
        break;
    case VgmColorModeRgbBacklight:
        rgb_backlight_get_color(0, &color);
        break;
    default:
        break;
    }

    return color;
}
