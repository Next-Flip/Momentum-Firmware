#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union __attribute__((packed)) {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
    uint32_t value : 24;
} RgbColor;

typedef union __attribute__((packed)) {
    struct {
        uint8_t h;
        uint8_t s;
        uint8_t v;
    };
    uint32_t value : 24;
} HsvColor;

int rgbcmp(const RgbColor* a, const RgbColor* b);
int hsvcmp(const HsvColor* a, const HsvColor* b);

void hsv2rgb(const HsvColor* hsv, RgbColor* rgb);
void rgb2hsv(const RgbColor* rgb, HsvColor* hsv);

RgbColor interpolate_color(
    const RgbColor* colorStart,
    const RgbColor* colorEnd,
    uint8_t step,
    uint8_t maxSteps);

RgbColor lerp_color(const RgbColor* colorStart, const RgbColor* colorEnd, double t);

#ifdef __cplusplus
}
#endif
