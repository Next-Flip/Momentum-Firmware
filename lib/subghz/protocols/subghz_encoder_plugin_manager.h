#pragma once

#include "subghz_encoder_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file subghz_encoder_plugin_manager.h
 * @brief Manages loading/unloading a single SubGHz encoder FAL from SD card.
 *
 * Lifetime:
 *   alloc   -> subghz_txrx_alloc()
 *   load    -> subghz_scene_receiver_info_on_enter() (after decode)
 *   unload  -> subghz_scene_receiver_info_on_exit()  (when result cleared)
 *   free    -> subghz_txrx_free()
 *
 * Only one encoder lives in RAM at a time. Same-protocol loads are cache hits.
 */

#define SUBGHZ_ENCODER_FAL_DIR EXT_PATH("apps_data/subghz/plugins")

typedef struct SubGhzEncoderPluginManager SubGhzEncoderPluginManager;

SubGhzEncoderPluginManager* subghz_encoder_plugin_manager_alloc(void);
void subghz_encoder_plugin_manager_free(SubGhzEncoderPluginManager* manager);

bool subghz_encoder_plugin_manager_load(
    SubGhzEncoderPluginManager* manager,
    const char* protocol_name);

void subghz_encoder_plugin_manager_unload(SubGhzEncoderPluginManager* manager);

const SubGhzEncoderPlugin* subghz_encoder_plugin_manager_get(SubGhzEncoderPluginManager* manager);

bool subghz_encoder_plugin_manager_is_loaded(SubGhzEncoderPluginManager* manager);

#ifdef __cplusplus
}
#endif
