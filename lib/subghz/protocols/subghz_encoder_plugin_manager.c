#include "subghz_encoder_plugin_manager.h"

#include <lib/flipper_application/plugins/plugin_manager.h>
#include <storage/storage.h>
#include <furi.h>

#define TAG "SubGhzEncMgr"

struct SubGhzEncoderPluginManager {
    PluginManager* pm;
    const SubGhzEncoderPlugin* plugin;
    char loaded_protocol[64];
};

SubGhzEncoderPluginManager* subghz_encoder_plugin_manager_alloc(void) {
    SubGhzEncoderPluginManager* m = malloc(sizeof(SubGhzEncoderPluginManager));
    furi_assert(m);
    m->pm = NULL;
    m->plugin = NULL;
    m->loaded_protocol[0] = '\0';
    return m;
}

void subghz_encoder_plugin_manager_free(SubGhzEncoderPluginManager* manager) {
    furi_assert(manager);
    subghz_encoder_plugin_manager_unload(manager);
    free(manager);
}
bool subghz_encoder_plugin_manager_load(
    SubGhzEncoderPluginManager* manager,
    const char* protocol_name) {
    furi_assert(manager);
    furi_assert(protocol_name);

    if(manager->pm && strncmp(manager->loaded_protocol, protocol_name, 63) == 0) {
        FURI_LOG_D(TAG, "Cache hit '%s'", protocol_name);
        return true;
    }

    subghz_encoder_plugin_manager_unload(manager);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error sd_status = storage_sd_status(storage);
    furi_record_close(RECORD_STORAGE);
    if(sd_status != FSE_OK) {
        FURI_LOG_W(TAG, "SD not ready (status=%d), encoder load skipped", sd_status);
        return false;
    }

    char path[128];
    char fal_name[64];
    if(strcmp(protocol_name, "Security+ 2.0") == 0) {
        strncpy(fal_name, "subghz_encoder_secplus_v2", sizeof(fal_name) - 1);
    } else {
        strncpy(fal_name, protocol_name, sizeof(fal_name) - 1);
    }
    fal_name[sizeof(fal_name) - 1] = '\0';
    snprintf(path, sizeof(path), "%s/%s.fal", SUBGHZ_ENCODER_FAL_DIR, fal_name);
    FURI_LOG_I(TAG, "Loading %s", path);

    manager->pm = plugin_manager_alloc(
        SUBGHZ_ENCODER_PLUGIN_APP_ID, SUBGHZ_ENCODER_PLUGIN_API_VERSION, NULL);

    PluginManagerError err = plugin_manager_load_single(manager->pm, path);
    if(err != PluginManagerErrorNone) {
        FURI_LOG_W(TAG, "Load failed err=%d path=%s", err, path);
        plugin_manager_free(manager->pm);
        manager->pm = NULL;
        return false;
    }

    manager->plugin = (const SubGhzEncoderPlugin*)plugin_manager_get_ep(manager->pm, 0);
    if(!manager->plugin) {
        FURI_LOG_E(TAG, "No entry point in FAL for '%s'", protocol_name);
        plugin_manager_free(manager->pm);
        manager->pm = NULL;
        return false;
    }

    strncpy(manager->loaded_protocol, protocol_name, 63);
    manager->loaded_protocol[63] = '\0';
    FURI_LOG_I(TAG, "Loaded encoder for '%s'", protocol_name);
    return true;
}

void subghz_encoder_plugin_manager_unload(SubGhzEncoderPluginManager* manager) {
    furi_assert(manager);
    if(!manager->pm) return;
    FURI_LOG_I(TAG, "Unloading encoder '%s'", manager->loaded_protocol);
    plugin_manager_free(manager->pm);
    manager->pm = NULL;
    manager->plugin = NULL;
    manager->loaded_protocol[0] = '\0';
}

const SubGhzEncoderPlugin* subghz_encoder_plugin_manager_get(SubGhzEncoderPluginManager* manager) {
    if(!manager || !manager->pm) return NULL;
    return manager->plugin;
}

bool subghz_encoder_plugin_manager_is_loaded(SubGhzEncoderPluginManager* manager) {
    return manager && manager->pm;
}
