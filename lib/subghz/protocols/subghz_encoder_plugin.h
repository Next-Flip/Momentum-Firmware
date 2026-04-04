#pragma once

#include "base.h"
#include <flipper_format/flipper_format.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file subghz_encoder_plugin.h
 * @brief SubGHz encoder plugin interface for SD-card dynamic loading.
 *
 * Each encodable SubGHz protocol ships its encoder as a PLUGIN FAL at:
 *   /ext/apps_data/subghz/plugins/<Protocol Name>.fal
 *
 * The FAL entry_point function returns a FlipperAppPluginDescriptor whose
 * entry_point field points to a SubGhzEncoderPlugin vtable.
 *
 * IMPORTANT: yield() is called from a hardware timer ISR.
 * The FAL is mapped to RAM by plugin_manager so all function pointers are
 * safe to call from ISR context. No blocking, no heap alloc inside yield().
 */

#define SUBGHZ_ENCODER_PLUGIN_APP_ID      "SubGhzEncoderPlugin"
#define SUBGHZ_ENCODER_PLUGIN_API_VERSION 1

typedef struct {
    SubGhzProtocolEncoderBase* (*alloc)(SubGhzEnvironment* environment);
    void (*free)(void* context);
    SubGhzProtocolStatus (*deserialize)(void* context, FlipperFormat* flipper_format);
    void (*stop)(void* context);
    LevelDuration (*yield)(void* context);
} SubGhzEncoderPlugin;

#ifdef __cplusplus
}
#endif
