#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz_i.h>
#include <toolbox/run_parallel.h>
#include <subghz/subghz_last_settings.h>
#include <flipper_format/flipper_format_i.h>

#define TAG "SubGhzExtendedFreq"

static int32_t subghz_extended_freq_apply(void* context) {
    UNUSED(context);
    FURI_LOG_D(TAG, "Loading settings");

    bool is_extended_i = false;
    bool is_bypassed = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(file, "/ext/subghz/assets/extend_range.txt")) {
        flipper_format_read_bool(file, "use_ext_range_at_own_risk", &is_extended_i, 1);
        flipper_format_read_bool(file, "ignore_default_tx_region", &is_bypassed, 1);
        flipper_format_file_close(file);
    }

    furi_hal_subghz_set_extended_range(is_extended_i);
    furi_hal_subghz_set_bypass_region(is_bypassed);

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

static void subghz_extended_freq_mount_callback(const void* message, void* context) {
    UNUSED(context);
    const StorageEvent* event = message;

    if(event->type == StorageEventTypeCardMount) {
        run_parallel(subghz_extended_freq_apply, NULL, 1024);
    }
}

void subghz_extended_freq() {
    if(!furi_hal_is_normal_boot()) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    furi_pubsub_subscribe(storage_get_pubsub(storage), subghz_extended_freq_mount_callback, NULL);

    if(storage_sd_status(storage) != FSE_OK) {
        FURI_LOG_D(TAG, "SD Card not ready, skipping settings");
    } else {
        subghz_extended_freq_apply(NULL);
    }

    furi_record_close(RECORD_STORAGE);
}
