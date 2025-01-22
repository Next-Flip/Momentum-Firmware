#include "flipper.h"
#include <applications.h>
#include <furi.h>
#include <furi_hal_version.h>
#include <furi_hal_memory.h>
#include <furi_hal_light.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <gui/canvas_i.h>

#include <FreeRTOS.h>

#define TAG "Flipper"

static void flipper_print_version(const char* target, const Version* version) {
    if(version) {
        FURI_LOG_I(
            TAG,
            "\r\n\t%s version:\t%s\r\n"
            "\tBuild date:\t\t%s\r\n"
            "\tGit Commit:\t\t%s (%s)%s\r\n"
            "\tGit Branch:\t\t%s",
            target,
            version_get_version(version),
            version_get_builddate(version),
            version_get_githash(version),
            version_get_gitbranchnum(version),
            version_get_dirty_flag(version) ? " (dirty)" : "",
            version_get_gitbranch(version));
    } else {
        FURI_LOG_I(TAG, "No build info for %s", target);
    }
}

#ifndef FURI_RAM_EXEC
#include <furi_hal.h>
#include <assets_icons.h>
#include <momentum/asset_packs.h>
#include <momentum/namespoof.h>
#include <momentum/settings_i.h>

#include <applications/main/archive/helpers/archive_favorites.h>
#include <bt/bt_service/bt_keys_filename.h>
#include <bt/bt_settings_filename.h>
#include <desktop/desktop_keybinds_filename.h>
#include <desktop/desktop_settings_filename.h>
#include <dolphin/helpers/dolphin_state_filename.h>
#include <expansion/expansion_settings_filename.h>
#include <loader/loader_menu.h>
#include <notification/notification_settings_filename.h>
#include <power/power_settings_filename.h>
#include <drivers/rgb_backlight_filename.h>
#include <applications/main/infrared/infrared_settings.h>
#include <applications/main/u2f/u2f_data.h>

void flipper_migrate_files() {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Revert cringe
    FURI_LOG_I(TAG, "Migrate: Remove unused files");
    storage_common_remove(storage, INT_PATH(".passport.settings"));

    // Migrate files
    FURI_LOG_I(TAG, "Migrate: Rename old paths");
    // If multiple have same destination, first match that exists is kept and others deleted
    const struct {
        const char* src;
        const char* dst;
    } renames[] = {
        // Renames on Ext
        {EXT_PATH(".config/favorites.txt"), ARCHIVE_FAV_PATH}, // Adapt to OFW/UL
        // Ext -> "Int"
        {EXT_PATH(".config/bt.keys"), BT_KEYS_STORAGE_PATH},
        {EXT_PATH(".config/bt.settings"), BT_SETTINGS_PATH},
        {EXT_PATH(".config/desktop.keybinds"), DESKTOP_KEYBINDS_PATH_MIGRATE},
        {EXT_PATH(".config/.desktop.keybinds"), DESKTOP_KEYBINDS_PATH_MIGRATE}, // Old naming
        {EXT_PATH(".config/desktop.settings"), DESKTOP_SETTINGS_PATH},
        {EXT_PATH(".config/dolphin.state"), DOLPHIN_STATE_PATH},
        {EXT_PATH(".config/expansion.settings"), EXPANSION_SETTINGS_PATH},
        {EXT_PATH(".config/mainmenu_apps.txt"), MAINMENU_APPS_PATH},
        {EXT_PATH(".config/xtreme_menu.txt"), MAINMENU_APPS_PATH},
        {EXT_PATH(".config/momentum_settings.txt"), MOMENTUM_SETTINGS_PATH},
        {EXT_PATH(".config/xtreme_settings.txt"), MOMENTUM_SETTINGS_PATH},
        {EXT_PATH(".config/notification.settings"), NOTIFICATION_SETTINGS_PATH},
        {EXT_PATH(".config/power.settings"), POWER_SETTINGS_PATH},
        {EXT_PATH(".config/rgb_backlight.settings"), RGB_BACKLIGHT_SETTINGS_PATH},
        {EXT_PATH("dolphin/name.txt"), NAMESPOOF_PATH}, // Adapt to UL
        {EXT_PATH("infrared/.infrared.settings"), INFRARED_SETTINGS_PATH}, // Adapt to OFW
    };
    for(size_t i = 0; i < COUNT_OF(renames); ++i) {
        // Use copy+remove to not overwrite dst but still delete src
        storage_common_copy(storage, renames[i].src, renames[i].dst);
        storage_common_remove(storage, renames[i].src);
    }

    // Int -> Ext for U2F
    FURI_LOG_I(TAG, "Migrate: U2F");
    if(storage_common_exists(storage, INT_PATH(".cnt.u2f"))) {
        const char* cnt_dst = storage_common_exists(storage, U2F_CNT_FILE) ? U2F_CNT_FILE ".old" :
                                                                             U2F_CNT_FILE;
        storage_common_rename(storage, INT_PATH(".cnt.u2f"), cnt_dst);
    }
    if(storage_common_exists(storage, INT_PATH(".key.u2f"))) {
        const char* key_dst = storage_common_exists(storage, U2F_KEY_FILE) ? U2F_KEY_FILE ".old" :
                                                                             U2F_KEY_FILE;
        storage_common_rename(storage, INT_PATH(".key.u2f"), key_dst);
    }

    // Remove obsolete .config folder after migration
    if(!storage_simply_remove(storage, EXT_PATH(".config"))) {
        FURI_LOG_W(TAG, "Can't remove /ext/.config/, probably not empty");
    }

    // Asset packs migrate, merges together
    FURI_LOG_I(TAG, "Migrate: Asset Packs");
    storage_common_migrate(storage, EXT_PATH("dolphin_custom"), ASSET_PACKS_PATH);

    furi_record_close(RECORD_STORAGE);
}

// Cannot use pubsub to schedule on SD insert like other services because
// we want migration to happen before others load settings and new pubsub
// subscriptions are put first in callback list so migration would be last
// Also we cannot block the pubsub by loading files as it means storage
// service is deadlocked processing pubsub and cannot process file operations
// So instead storage runs this function in background thread and then
// dispatches the pubsub event to everyone else
bool skip_double_mount = false;
void flipper_mount_callback(const void* message, void* context) {
    UNUSED(context);
    const StorageEvent* event = message;

    if(event->type == StorageEventTypeCardMount) {
        // Workaround to avoid double load on boot but also have animated boot screen
        if(skip_double_mount) {
            skip_double_mount = false;
            return;
        }

        // Migrate locations before other services load on SD insert
        flipper_migrate_files();

        // TODO: Need to restart services that already applied previous name
        namespoof_init();

        // TODO: If new SD doesn't contain all current settings IDs, values
        // from previous SD are kept for these settings
        momentum_settings_load();

        // TODO: Could lock kernel on free to avoid GUI using assets while being free'd
        asset_packs_free();
        asset_packs_init();
    }
}
#endif

void flipper_start_service(const FlipperInternalApplication* service) {
    FURI_LOG_D(TAG, "Starting service %s", service->name);

    FuriThread* thread =
        furi_thread_alloc_service(service->name, service->stack_size, service->app, NULL);
    furi_thread_set_appid(thread, service->appid);

    furi_thread_start(thread);
}

void flipper_init(void) {
    furi_hal_light_sequence("rgb WB");
    flipper_print_version("Firmware", furi_hal_version_get_firmware_version());
    FURI_LOG_I(TAG, "Boot mode %d", furi_hal_rtc_get_boot_mode());

#ifndef FURI_RAM_EXEC
    Canvas* canvas = canvas_init();
    canvas_draw_icon(canvas, 33, 16, &I_Updating_Logo_62x15);
    if(furi_hal_is_normal_boot()) {
        canvas_draw_icon(canvas, 19, 44, &I_SDcardMounted_11x8);
    }
    canvas_commit(canvas);
#endif

    // Start storage service first, thanks OFW :/
    flipper_start_service(&FLIPPER_SERVICES[0]);

#ifndef FURI_RAM_EXEC
    if(furi_hal_is_normal_boot()) {
        // Wait for storage record
        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(storage_sd_status(storage) != FSE_OK) {
            FURI_LOG_D(TAG, "SD Card not ready, skipping early init");
            // Init on SD insert done by storage using flipper_mount_callback()
        } else {
            // Workaround to avoid double load on boot but also have animated boot screen
            skip_double_mount = true;

            canvas_draw_icon(canvas, 39, 43, &I_dir_10px);
            canvas_commit(canvas);
            flipper_migrate_files();

            canvas_draw_icon(canvas, 59, 42, &I_Apps_10px);
            canvas_commit(canvas);
            namespoof_init();

            canvas_draw_icon(canvas, 79, 44, &I_Rpc_active_7x8);
            canvas_commit(canvas);
            momentum_settings_load();

            furi_hal_light_sequence("rgb RB");
            canvas_draw_icon(canvas, 99, 44, &I_Hidden_window_9x8);
            canvas_commit(canvas);
            asset_packs_init();
        }
        furi_record_close(RECORD_STORAGE);
    } else {
        FURI_LOG_I(TAG, "Special boot, skipping optional components");
    }
#endif

    // Everything else
    for(size_t i = 1; i < FLIPPER_SERVICES_COUNT; i++) {
        flipper_start_service(&FLIPPER_SERVICES[i]);
    }

#ifndef FURI_RAM_EXEC
    canvas_free(canvas);
#endif

    FURI_LOG_I(TAG, "Startup complete");
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configIDLE_TASK_STACK_DEPTH);
    *stack_size = configIDLE_TASK_STACK_DEPTH;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configTIMER_TASK_STACK_DEPTH);
    *stack_size = configTIMER_TASK_STACK_DEPTH;
}
