#include "asset_packs.h"

typedef struct {
    uint8_t* fonts[FontTotalNumber];
    CanvasFontParameters* font_params[FontTotalNumber];
} AssetPacks;

extern AssetPacks asset_packs;
