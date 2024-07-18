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
#include <applications/main/archive/helpers/archive_favorites.h>
#include <applications/main/infrared/infrared_app.h>
#include <applications/main/u2f/u2f_data.h>
#include <assets_icons.h>
#include <bt/bt_service/bt_i.h>
#include <bt/bt_settings.h>
#include <desktop/desktop_settings.h>
#include <dolphin/helpers/dolphin_state.h>
#include <expansion/expansion_settings_filename.h>
#include <loader/loader_menu.h>
#include <momentum/asset_packs.h>
#include <momentum/namespoof.h>
#include <momentum/settings_i.h>
#include <notification/notification_app.h>
#include <power/power_settings.h>

void flipper_migrate_files() {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Revert cringe
    FURI_LOG_I(TAG, "Migrate: Remove unused files");
    storage_common_remove(storage, INT_PATH(".passport.settings"));

    // Migrate files
    FURI_LOG_I(TAG, "Migrate: Rename old paths");
    const struct {
        const char* src;
        const char* dst;
        bool delete;
    } renames[] = {
        // Renames on ext
        {CFG_PATH("favorites.txt"), ARCHIVE_FAV_PATH, true}, // Adapt to OFW/UL
        {CFG_PATH(".desktop.keybinds"), DESKTOP_KEYBINDS_PATH, true}, // Old naming
        {CFG_PATH("xtreme_menu.txt"), MAINMENU_APPS_PATH, false}, // Keep both
        {CFG_PATH("xtreme_settings.txt"), MOMENTUM_SETTINGS_PATH, false}, // Keep both
        // Int -> Ext
        {INT_PATH(".bt.settings"), BT_SETTINGS_PATH, true},
        {INT_PATH(".dolphin.state"), DOLPHIN_STATE_PATH, true},
        {INT_PATH(".power.settings"), POWER_SETTINGS_PATH, true},
        {INT_PATH(".bt.keys"), BT_KEYS_STORAGE_PATH, true},
        {INT_PATH(".expansion.settings"), EXPANSION_SETTINGS_PATH, true},
        {INT_PATH(".notification.settings"), NOTIFICATION_SETTINGS_PATH, true},
        {INT_PATH(".infrared.settings"), INFRARED_SETTINGS_PATH, true},
        // Ext -> Int
        {CFG_PATH("desktop.settings"), DESKTOP_SETTINGS_PATH, true},
    };
    for(size_t i = 0; i < COUNT_OF(renames); ++i) {
        // Use copy+remove to not overwrite dst but still delete src
        storage_common_copy(storage, renames[i].src, renames[i].dst);
        if(renames[i].delete) {
            storage_common_remove(storage, renames[i].src);
        }
    }

    // Special care for U2F
    FURI_LOG_I(TAG, "Migrate: U2F");
    FileInfo file_info;
    if(storage_common_stat(storage, INT_PATH(".cnt.u2f"), &file_info) == FSE_OK &&
       file_info.size > 200) { // Is on Int and has content
        storage_common_rename(storage, INT_PATH(".cnt.u2f"), U2F_CNT_FILE); // Int -> Ext
    }
    storage_common_copy(storage, U2F_DATA_FOLDER "key.u2f", U2F_KEY_FILE); // Ext -> Int

    // Asset packs migrate, merges together
    FURI_LOG_I(TAG, "Migrate: Asset Packs");
    storage_common_migrate(storage, EXT_PATH("dolphin_custom"), ASSET_PACKS_PATH);

    furi_record_close(RECORD_STORAGE);
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
        furi_record_open(RECORD_STORAGE);
        furi_record_close(RECORD_STORAGE);

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
