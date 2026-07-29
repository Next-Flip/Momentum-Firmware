#include "level99_can_config.h"

#include <toolbox/saved_struct.h>

#define LEVEL99_CAN_CONFIG_PATH    APP_DATA_PATH("config.bin")
#define LEVEL99_CAN_CONFIG_MAGIC   0x99U
#define LEVEL99_CAN_CONFIG_VERSION 1U

void level99_can_config_set_defaults(Level99CanConfig* config) {
    furi_check(config);
    memset(config, 0, sizeof(*config));
    config->version = LEVEL99_CAN_CONFIG_VERSION;
    config->oscillator = Level99CanOscillator8MHz;
    config->bitrate = Level99CanBitrate500K;
    config->default_mode = Level99CanModeListenOnly;
    config->compact_display = true;
    config->filter_id_max = 0x1FFFFFFFUL;
    config->id_filter = Level99CanIdAny;
    config->rtr_filter = Level99CanFrameAny;
}

static bool level99_can_bitrate_valid(Level99CanBitrate bitrate) {
    switch(bitrate) {
    case Level99CanBitrate10K:
    case Level99CanBitrate20K:
    case Level99CanBitrate50K:
    case Level99CanBitrate100K:
    case Level99CanBitrate125K:
    case Level99CanBitrate250K:
    case Level99CanBitrate500K:
    case Level99CanBitrate1000K:
        return true;
    default:
        return false;
    }
}

bool level99_can_config_is_valid(const Level99CanConfig* config) {
    return config && config->version == LEVEL99_CAN_CONFIG_VERSION &&
           (config->oscillator == Level99CanOscillator8MHz ||
            config->oscillator == Level99CanOscillator16MHz) &&
           level99_can_bitrate_valid(config->bitrate) &&
           (config->default_mode == Level99CanModeListenOnly ||
            config->default_mode == Level99CanModeLoopback) &&
           config->filter_id_min <= config->filter_id_max &&
           config->filter_id_max <= 0x1FFFFFFFUL && config->id_filter <= Level99CanIdExtended &&
           config->rtr_filter <= Level99CanFrameRtr;
}

bool level99_can_config_load(Storage* storage, Level99CanConfig* config) {
    UNUSED(storage);
    level99_can_config_set_defaults(config);
    Level99CanConfig loaded;
    if(!saved_struct_load(
           LEVEL99_CAN_CONFIG_PATH,
           &loaded,
           sizeof(loaded),
           LEVEL99_CAN_CONFIG_MAGIC,
           LEVEL99_CAN_CONFIG_VERSION) ||
       !level99_can_config_is_valid(&loaded)) {
        return false;
    }
    *config = loaded;
    return true;
}

bool level99_can_config_save(Storage* storage, const Level99CanConfig* config) {
    UNUSED(storage);
    if(!level99_can_config_is_valid(config)) return false;
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    return saved_struct_save(
        LEVEL99_CAN_CONFIG_PATH,
        config,
        sizeof(*config),
        LEVEL99_CAN_CONFIG_MAGIC,
        LEVEL99_CAN_CONFIG_VERSION);
}
