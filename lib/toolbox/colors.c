// https://stackoverflow.com/a/14733008

#include "colors.h"

_Static_assert(sizeof(RgbColor) == 3, "RGB color must be 24-bit");
_Static_assert(sizeof(HsvColor) == 3, "HSV color must be 24-bit");

inline int rgbcmp(const RgbColor* a, const RgbColor* b) {
    return memcmp(a, b, sizeof(RgbColor));
}

inline int hsvcmp(const HsvColor* a, const HsvColor* b) {
    return memcmp(a, b, sizeof(HsvColor));
}

void hsv2rgb(const HsvColor* hsv, RgbColor* rgb) {
    if(hsv->s == 0) {
        rgb->r = hsv->v;
        rgb->g = hsv->v;
        rgb->b = hsv->v;
        return;
    }

    uint8_t region = hsv->h / 43;
    uint8_t remainder = (hsv->h - (region * 43)) * 6;

    uint8_t p = (hsv->v * (255 - hsv->s)) >> 8;
    uint8_t q = (hsv->v * (255 - ((hsv->s * remainder) >> 8))) >> 8;
    uint8_t t = (hsv->v * (255 - ((hsv->s * (255 - remainder)) >> 8))) >> 8;

    switch(region) {
    case 0:
        rgb->r = hsv->v;
        rgb->g = t;
        rgb->b = p;
        break;
    case 1:
        rgb->r = q;
        rgb->g = hsv->v;
        rgb->b = p;
        break;
    case 2:
        rgb->r = p;
        rgb->g = hsv->v;
        rgb->b = t;
        break;
    case 3:
        rgb->r = p;
        rgb->g = q;
        rgb->b = hsv->v;
        break;
    case 4:
        rgb->r = t;
        rgb->g = p;
        rgb->b = hsv->v;
        break;
    default:
        rgb->r = hsv->v;
        rgb->g = p;
        rgb->b = q;
        break;
    }
}

void rgb2hsv(const RgbColor* rgb, HsvColor* hsv) {
    uint8_t rgbMin = rgb->r < rgb->g ? (rgb->r < rgb->b ? rgb->r : rgb->b) :
                                       (rgb->g < rgb->b ? rgb->g : rgb->b);
    uint8_t rgbMax = rgb->r > rgb->g ? (rgb->r > rgb->b ? rgb->r : rgb->b) :
                                       (rgb->g > rgb->b ? rgb->g : rgb->b);

    hsv->v = rgbMax;
    if(hsv->v == 0) {
        hsv->h = 0;
        hsv->s = 0;
        return;
    }

    hsv->s = 255 * ((long)rgbMax - (long)rgbMin) / hsv->v;
    if(hsv->s == 0) {
        hsv->h = 0;
        return;
    }

    if(rgbMax == rgb->r) {
        hsv->h = 0 + 43 * (rgb->g - rgb->b) / (rgbMax - rgbMin);
    } else if(rgbMax == rgb->g) {
        hsv->h = 85 + 43 * (rgb->b - rgb->r) / (rgbMax - rgbMin);
    } else {
        hsv->h = 171 + 43 * (rgb->r - rgb->g) / (rgbMax - rgbMin);
    }
}

RgbColor interpolate_color(
    const RgbColor* colorStart,
    const RgbColor* colorEnd,
    uint8_t step,
    uint8_t maxSteps) {
    RgbColor color;

    if(step >= maxSteps) {
        memcpy(&color, colorEnd, sizeof(RgbColor));
        return color;
    }

    color.r = colorStart->r + (colorEnd->r - colorStart->r) * step / maxSteps;
    color.g = colorStart->g + (colorEnd->g - colorStart->g) * step / maxSteps;
    color.b = colorStart->b + (colorEnd->b - colorStart->b) * step / maxSteps;

    return color;
}

RgbColor lerp_color(const RgbColor* colorStart, const RgbColor* colorEnd, double t) {
    RgbColor color;

    color.r = colorStart->r * ((double)1.0 - t) + colorEnd->r * t;
    color.g = colorStart->g * ((double)1.0 - t) + colorEnd->g * t;
    color.b = colorStart->b * ((double)1.0 - t) + colorEnd->b * t;

    return color;
}
