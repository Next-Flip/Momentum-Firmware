#include "dolphin_state.h"
#include "dolphin_state_filename.h"

#include <furi.h>
#include <furi_hal.h>

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define TAG "DolphinState"

#define DOLPHIN_STATE_HEADER_MAGIC   0xD0
#define DOLPHIN_STATE_HEADER_VERSION 0x01

/*
 * The original Momentum/Flipper thresholds through level 30 are preserved.
 * Level99 extends progression in conservative 600-XP steps. A value above
 * the final threshold is level 99, matching the existing calculation model.
 */
const uint32_t DOLPHIN_LEVELS[] = {
    100,   200,   300,   450,   600,   750,   950,   1150,  1350,  1600,  1850,  2100,  2400,
    2700,  3000,  3350,  3700,  4050,  4450,  4850,  5250,  5700,  6150,  6600,  7100,  7600,
    8100,  8650,  9999,  10599, 11199, 11799, 12399, 12999, 13599, 14199, 14799, 15399, 15999,
    16599, 17199, 17799, 18399, 18999, 19599, 20199, 20799, 21399, 21999, 22599, 23199, 23799,
    24399, 24999, 25599, 26199, 26799, 27399, 27999, 28599, 29199, 29799, 30399, 30999, 31599,
    32199, 32799, 33399, 33999, 34599, 35199, 35799, 36399, 36999, 37599, 38199, 38799, 39399,
    39999, 40599, 41199, 41799, 42399, 42999, 43599, 44199, 44799, 45399, 45999, 46599, 47199,
    47799, 48399, 48999, 49599, 50199, 50799, 51399};
const size_t DOLPHIN_LEVEL_COUNT = COUNT_OF(DOLPHIN_LEVELS);

DolphinState* dolphin_state_alloc(void) {
    return malloc(sizeof(DolphinState));
}

void dolphin_state_free(DolphinState* dolphin_state) {
    free(dolphin_state);
}

void dolphin_state_save(DolphinState* dolphin_state) {
    if(!dolphin_state->dirty) {
        return;
    }

    bool success = saved_struct_save(
        DOLPHIN_STATE_PATH,
        &dolphin_state->data,
        sizeof(DolphinStoreData),
        DOLPHIN_STATE_HEADER_MAGIC,
        DOLPHIN_STATE_HEADER_VERSION);

    if(success) {
        FURI_LOG_I(TAG, "State saved");
        dolphin_state->dirty = false;

    } else {
        FURI_LOG_E(TAG, "Failed to save state");
    }
}

void dolphin_state_load(DolphinState* dolphin_state) {
    bool success = saved_struct_load(
        DOLPHIN_STATE_PATH,
        &dolphin_state->data,
        sizeof(DolphinStoreData),
        DOLPHIN_STATE_HEADER_MAGIC,
        DOLPHIN_STATE_HEADER_VERSION);

    if(success) {
        if((dolphin_state->data.butthurt > BUTTHURT_MAX) ||
           (dolphin_state->data.butthurt < BUTTHURT_MIN)) {
            success = false;
        }
    }

    if(!success) {
        FURI_LOG_W(TAG, "Reset Dolphin state");
        memset(dolphin_state, 0, sizeof(DolphinState));

        dolphin_state->data.icounter = dolphin_state_level_minimum_xp(99);
        dolphin_state->dirty = true;
        // dolphin_state_save(dolphin_state);
    }
}

uint64_t dolphin_state_timestamp(void) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    return datetime_datetime_to_timestamp(&datetime);
}

bool dolphin_state_is_levelup(uint32_t icounter) {
    for(size_t i = 0; i < DOLPHIN_LEVEL_COUNT; ++i) {
        if((icounter == DOLPHIN_LEVELS[i])) {
            return true;
        }
    }
    return false;
}

uint8_t dolphin_get_level(uint32_t icounter) {
    for(size_t i = 0; i < DOLPHIN_LEVEL_COUNT; ++i) {
        if(icounter <= DOLPHIN_LEVELS[i]) {
            return i + 1;
        }
    }
    return DOLPHIN_LEVEL_COUNT + 1;
}

uint32_t dolphin_state_level_minimum_xp(uint8_t level) {
    if(level <= 1U) {
        return 0U;
    }

    const size_t previous_level_index = (size_t)level - 2U;
    if(previous_level_index >= DOLPHIN_LEVEL_COUNT) {
        return DOLPHIN_LEVELS[DOLPHIN_LEVEL_COUNT - 1U] + 1U;
    }

    return DOLPHIN_LEVELS[previous_level_index] + 1U;
}

uint32_t dolphin_state_xp_above_last_levelup(uint32_t icounter) {
    uint8_t level_idx = dolphin_get_level(icounter) - 1; // Level = index + 1
    if(level_idx > 0) {
        return icounter - DOLPHIN_LEVELS[level_idx - 1]; // Get prev level
    }
    return icounter;
}

uint32_t dolphin_state_xp_to_levelup(uint32_t icounter) {
    uint8_t level_idx = dolphin_get_level(icounter) - 1; // Level = index + 1
    if(level_idx < DOLPHIN_LEVEL_COUNT) {
        return DOLPHIN_LEVELS[level_idx] - icounter;
    }
    return (uint32_t)-1;
}

void dolphin_state_on_deed(DolphinState* dolphin_state, DolphinDeed deed) {
    // Special case for testing
    if(deed > DolphinDeedMAX) {
        if(deed == DolphinDeedTestLeft) {
            dolphin_state->data.butthurt =
                CLAMP(dolphin_state->data.butthurt + 1, BUTTHURT_MAX, BUTTHURT_MIN);
            if(dolphin_state->data.icounter > 0) dolphin_state->data.icounter--;
            dolphin_state->data.timestamp = dolphin_state_timestamp();
            dolphin_state->dirty = true;
        } else if(deed == DolphinDeedTestRight) {
            dolphin_state->data.butthurt = BUTTHURT_MIN;
            if(dolphin_state->data.icounter < UINT32_MAX) dolphin_state->data.icounter++;
            dolphin_state->data.timestamp = dolphin_state_timestamp();
            dolphin_state->dirty = true;
        }
        return;
    }

    DolphinApp app = dolphin_deed_get_app(deed);
    int8_t weight_limit =
        dolphin_deed_get_app_limit(app) - dolphin_state->data.icounter_daily_limit[app];
    uint8_t deed_weight = CLAMP(dolphin_deed_get_weight(deed), weight_limit, 0);

    uint32_t xp_to_levelup = dolphin_state_xp_to_levelup(dolphin_state->data.icounter);
    if(xp_to_levelup) {
        deed_weight = MIN(xp_to_levelup, deed_weight);
        dolphin_state->data.icounter += deed_weight;
        dolphin_state->data.icounter_daily_limit[app] += deed_weight;
    }

    /* decrease butthurt:
     * 0 deeds accumulating --> 0 butthurt
     * +1....+15 deeds accumulating --> -1 butthurt
     * +16...+30 deeds accumulating --> -1 butthurt
     * +31...+45 deeds accumulating --> -1 butthurt
     * +46...... deeds accumulating --> -1 butthurt
     * -4 butthurt per day is maximum
     * */
    uint8_t butthurt_icounter_level_old = dolphin_state->data.butthurt_daily_limit / 15 +
                                          !!(dolphin_state->data.butthurt_daily_limit % 15);
    dolphin_state->data.butthurt_daily_limit =
        CLAMP(dolphin_state->data.butthurt_daily_limit + deed_weight, 46, 0);
    uint8_t butthurt_icounter_level_new = dolphin_state->data.butthurt_daily_limit / 15 +
                                          !!(dolphin_state->data.butthurt_daily_limit % 15);
    int32_t new_butthurt = ((int32_t)dolphin_state->data.butthurt) -
                           (butthurt_icounter_level_old != butthurt_icounter_level_new);
    new_butthurt = CLAMP(new_butthurt, BUTTHURT_MAX, BUTTHURT_MIN);

    dolphin_state->data.butthurt = new_butthurt;
    dolphin_state->data.timestamp = dolphin_state_timestamp();
    dolphin_state->dirty = true;

    FURI_LOG_D(
        TAG,
        "icounter %lu, butthurt %ld",
        dolphin_state->data.icounter,
        dolphin_state->data.butthurt);
}

void dolphin_state_butthurted(DolphinState* dolphin_state) {
    if(dolphin_state->data.butthurt < BUTTHURT_MAX) {
        dolphin_state->data.butthurt++;
        dolphin_state->data.timestamp = dolphin_state_timestamp();
        dolphin_state->dirty = true;
    }
}

void dolphin_state_increase_level(DolphinState* dolphin_state) {
    furi_assert(dolphin_state_is_levelup(dolphin_state->data.icounter));
    ++dolphin_state->data.icounter;
    dolphin_state->dirty = true;
}

void dolphin_state_clear_limits(DolphinState* dolphin_state) {
    furi_assert(dolphin_state);

    for(size_t i = 0; i < DolphinAppMAX; ++i) {
        dolphin_state->data.icounter_daily_limit[i] = 0;
    }
    dolphin_state->data.butthurt_daily_limit = 0;
    dolphin_state->dirty = true;
}
