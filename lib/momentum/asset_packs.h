#pragma once

#include <gui/canvas.h>
#include <stdint.h>

#define ASSET_PACKS_PATH EXT_PATH("asset_packs")

typedef struct {
    uint8_t* fonts[FontTotalNumber];
    CanvasFontParameters* font_params[FontTotalNumber];
} AssetPacks;

void asset_packs_init(void);
void asset_packs_free(void);
extern AssetPacks asset_packs;
