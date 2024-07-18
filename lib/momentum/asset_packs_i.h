#include "asset_packs.h"

#include <m-list.h>

typedef struct {
    const Icon* original;
    const Icon* replaced;
} IconSwap;

LIST_DEF(IconSwapList, IconSwap, M_POD_OPLIST)
#define M_OPL_IconSwapList_t() LIST_OPLIST(IconSwapList)

typedef struct {
    IconSwapList_t icons;
    uint8_t* fonts[FontTotalNumber];
    CanvasFontParameters* font_params[FontTotalNumber];
} AssetPacks;

extern AssetPacks* asset_packs;

const Icon* asset_packs_swap_icon(const Icon* requested);
