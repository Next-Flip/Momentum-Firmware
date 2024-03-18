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

typedef union {
    uint16_t value;
    struct {
        uint16_t r : 5;
        uint16_t g : 6;
        uint16_t b : 5;
    } FURI_PACKED;
} Rgb565Color;

int rgb565cmp(const Rgb565Color* a, const Rgb565Color* b);

#ifdef __cplusplus
}
#endif
