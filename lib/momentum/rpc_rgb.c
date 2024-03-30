#include "rpc_rgb.h"

#include "drivers/rgb_backlight.h"

uint32_t get_screen_color_fg() {
    RgbColorTransmit color = {
        .mode = momentum_settings.vgm_color_mode,
        .rgb = {{0, 0, 0}},
    };

    switch(momentum_settings.vgm_color_mode) {
    case VgmColorModeCustom:
        color.rgb = momentum_settings.vgm_color_fg;
        break;
    case VgmColorModeRgbBacklight:
        break;
    default:
        break;
    }

    return color.value;
}

uint32_t get_screen_color_bg() {
    RgbColorTransmit color = {
        .mode = momentum_settings.vgm_color_mode,
        .rgb = {{255, 130, 0}},
    };

    switch(momentum_settings.vgm_color_mode) {
    case VgmColorModeCustom:
        color.rgb = momentum_settings.vgm_color_bg;
        break;
    case VgmColorModeRgbBacklight:
        rgb_backlight_get_color(0, &color.rgb);
        break;
    default:
        break;
    }

    return color.value;
}
