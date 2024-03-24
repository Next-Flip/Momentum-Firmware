#pragma once

#include "colors.h"

#ifdef __cplusplus
extern "C" {
#endif

void rpc_rgb_init();

RgbColor get_screen_color_fg();
RgbColor get_screen_color_bg();

void rpc_rgb_deinit();

#ifdef __cplusplus
}
#endif
