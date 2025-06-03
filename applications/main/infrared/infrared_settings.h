#pragma once

#include <storage/storage.h>
#include <furi_hal_infrared.h>
#include <toolbox/saved_struct.h>

#define INFRARED_SETTINGS_PATH    INT_PATH(".infrared.settings")
#define INFRARED_SETTINGS_VERSION (2)
#define INFRARED_SETTINGS_MAGIC   (0x1F)

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
    bool easy_mode;
} InfraredSettings;

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
} _InfraredSettingsV1;

bool infrared_settings_load(InfraredSettings* settings) {
    // Default load
    if(saved_struct_load(
           INFRARED_SETTINGS_PATH,
           settings,
           sizeof(*settings),
           INFRARED_SETTINGS_MAGIC,
           INFRARED_SETTINGS_VERSION)) {
        return true;
    }

    // Set defaults
    settings->tx_pin = FuriHalInfraredTxPinInternal;
    settings->otg_enabled = false;
    settings->easy_mode = false;

    // Try to migrate
    uint8_t magic, version;
    if(saved_struct_get_metadata(INFRARED_SETTINGS_PATH, &magic, &version, NULL) &&
       magic == INFRARED_SETTINGS_MAGIC) {
        _InfraredSettingsV1 v1;

        if(version == 1 &&
           saved_struct_load(INFRARED_SETTINGS_PATH, &v1, sizeof(v1), magic, version)) {
            settings->tx_pin = v1.tx_pin;
            settings->otg_enabled = v1.otg_enabled;
            return true;
        }
    }

    return false;
}

bool infrared_settings_save(InfraredSettings* settings) {
    return saved_struct_save(
        INFRARED_SETTINGS_PATH,
        settings,
        sizeof(*settings),
        INFRARED_SETTINGS_MAGIC,
        INFRARED_SETTINGS_VERSION);
}
