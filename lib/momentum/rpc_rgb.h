#pragma once

#include "momentum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union __attribute__((packed)) {
    struct {
        VgmColorMode mode : 8;
        RgbColor rgb;
    };
    uint32_t value;
} RgbColorTransmit;

void rpc_rgb_init();

uint32_t get_screen_color_fg();
uint32_t get_screen_color_bg();

void rpc_rgb_deinit();

#ifdef __cplusplus
}
#endif
