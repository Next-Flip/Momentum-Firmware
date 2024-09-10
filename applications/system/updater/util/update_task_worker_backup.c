#include "update_task.h"
#include "update_task_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <desktop/views/desktop_view_slideshow.h>
#include <toolbox/path.h>
#include <update_util/dfu_file.h>
#include <update_util/int_backup.h>
#include <update_util/update_operation.h>
#include <update_util/resources/manifest.h>
#include <toolbox/tar/tar_archive.h>
#include <toolbox/crc32_calc.h>

#define FIRSTBOOT_FLAG_PATH INT_PATH(".momentum_firstboot.flag")

#define TAG "UpdWorkerBackup"

static bool update_task_pre_update(UpdateTask* update_task) {
    bool success = false;
    FuriString* backup_file_path;
    backup_file_path = furi_string_alloc();
    path_concat(
        furi_string_get_cstr(update_task->update_path),
        INT_BACKUP_DEFAULT_FILENAME,
        backup_file_path);

    update_task_set_progress(update_task, UpdateTaskStageIntBackup, 0);
    /* to avoid bootloops */
    furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeNormal);
    if((success =
            int_backup_create(update_task->storage, furi_string_get_cstr(backup_file_path)))) {
        furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeUpdate);
    }

    furi_string_free(backup_file_path);
    return success;
}

static void update_task_resource_unpack_cb(size_t progress, size_t total, void* context) {
    UpdateTask* update_task = context;
    update_task_set_progress(update_task, UpdateTaskStageProgress, (progress * 100) / (total + 1));
}

static void update_task_cleanup_resources(UpdateTask* update_task) {
    ResourceManifestReader* manifest_reader = resource_manifest_reader_alloc(update_task->storage);
    do {
        FURI_LOG_D(TAG, "Cleaning up old manifest");
        if(!resource_manifest_reader_open(manifest_reader, EXT_PATH("Manifest"))) {
            FURI_LOG_W(TAG, "No existing manifest");
            break;
        }

        ResourceManifestEntry* entry_ptr = NULL;
        /* Iterate over manifest and calculate entries count */
        uint32_t n_total_entries = 1;
        while((entry_ptr = resource_manifest_reader_next(manifest_reader))) {
            n_total_entries++;
        }
        resource_manifest_rewind(manifest_reader);

        update_task_set_progress(update_task, UpdateTaskStageResourcesFileCleanup, 0);
        uint32_t n_processed_file_entries = 0;
        while((entry_ptr = resource_manifest_reader_next(manifest_reader))) {
            update_task_set_progress(
                update_task,
                UpdateTaskStageProgress,
                (n_processed_file_entries++ * 100) / n_total_entries);
            if(entry_ptr->type == ResourceManifestEntryTypeFile) {
                FuriString* file_path = furi_string_alloc();
                path_concat(
                    STORAGE_EXT_PATH_PREFIX, furi_string_get_cstr(entry_ptr->name), file_path);
                FURI_LOG_D(TAG, "Removing %s", furi_string_get_cstr(file_path));

                FS_Error result =
                    storage_common_remove(update_task->storage, furi_string_get_cstr(file_path));
                if(result != FSE_OK && result != FSE_EXIST) {
                    FURI_LOG_E(
                        TAG,
                        "%s remove failed, cause %s",
                        furi_string_get_cstr(file_path),
                        storage_error_get_desc(result));
                }
                furi_string_free(file_path);
            }
        }

        update_task_set_progress(update_task, UpdateTaskStageResourcesDirCleanup, 0);
        uint32_t n_processed_dir_entries = 0;
        while((entry_ptr = resource_manifest_reader_previous(manifest_reader))) {
            update_task_set_progress(
                update_task,
                UpdateTaskStageProgress,
                (n_processed_dir_entries++ * 100) / n_total_entries);
            if(entry_ptr->type == ResourceManifestEntryTypeDirectory) {
                FuriString* folder_path = furi_string_alloc();

                do {
                    path_concat(
                        STORAGE_EXT_PATH_PREFIX,
                        furi_string_get_cstr(entry_ptr->name),
                        folder_path);

                    FURI_LOG_D(TAG, "Removing folder %s", furi_string_get_cstr(folder_path));
                    FS_Error result = storage_common_remove(
                        update_task->storage, furi_string_get_cstr(folder_path));
                    if(result != FSE_OK && result != FSE_EXIST) {
                        FURI_LOG_E(
                            TAG,
                            "%s remove failed, cause %s",
                            furi_string_get_cstr(folder_path),
                            storage_error_get_desc(result));
                    }
                } while(false);

                furi_string_free(folder_path);
            }
        }
    } while(false);
    resource_manifest_reader_free(manifest_reader);
}

static bool update_task_post_update(UpdateTask* update_task) {
    bool success = false;

    FuriString* file_path;
    file_path = furi_string_alloc();

    TarArchive* archive = tar_archive_alloc(update_task->storage);
    do {
        path_concat(
            furi_string_get_cstr(update_task->update_path),
            INT_BACKUP_DEFAULT_FILENAME,
            file_path);

        update_task_set_progress(update_task, UpdateTaskStageIntRestore, 0);

        CHECK_RESULT(int_backup_unpack(update_task->storage, furi_string_get_cstr(file_path)));

        // Fix flags for production / development
#ifdef FURI_DEBUG
        furi_hal_rtc_set_flag(FuriHalRtcFlagDebug);
        furi_hal_rtc_set_flag(FuriHalRtcFlagLegacySleep);
#else
        furi_hal_rtc_reset_flag(FuriHalRtcFlagDebug);
        furi_hal_rtc_reset_flag(FuriHalRtcFlagLegacySleep);
        furi_hal_rtc_set_log_level(FuriLogLevelNone);
        furi_hal_rtc_set_heap_track_mode(FuriHalRtcHeapTrackModeNone);
#endif

        if(update_task->state.groups & UpdateTaskStageGroupResources) {
            path_concat(
                furi_string_get_cstr(update_task->update_path),
                furi_string_get_cstr(update_task->manifest->resource_bundle),
                file_path);

            CHECK_RESULT(tar_archive_open(
                archive,
                furi_string_get_cstr(file_path),
                tar_archive_get_mode_for_path(furi_string_get_cstr(file_path))));

            update_task_cleanup_resources(update_task);

            update_task_set_progress(update_task, UpdateTaskStageResourcesFileUnpack, 0);
            tar_archive_set_read_callback(archive, update_task_resource_unpack_cb, update_task);
            CHECK_RESULT(tar_archive_unpack_to(archive, STORAGE_EXT_PATH_PREFIX, NULL));
        }

        if(update_task->state.groups & UpdateTaskStageGroupSplashscreen) {
            update_task_set_progress(update_task, UpdateTaskStageSplashscreenInstall, 0);
            FuriString* tmp_path;
            tmp_path = furi_string_alloc_set(update_task->update_path);
            storage_common_rename(
                update_task->storage,
                EXT_PATH(".config/firstboot.flag"), // Poor naming, shouldn't be generic for all FW
                FIRSTBOOT_FLAG_PATH);
            storage_common_rename(
                update_task->storage,
                EXT_PATH(".config/momentum_firstboot.flag"), // Migrate to int on ext
                FIRSTBOOT_FLAG_PATH);
            if(storage_common_stat(update_task->storage, FIRSTBOOT_FLAG_PATH, NULL) ==
               FSE_NOT_EXIST) {
                File* file = storage_file_alloc(update_task->storage);
                if(storage_file_open(file, FIRSTBOOT_FLAG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    storage_file_close(file);
                }
                storage_file_free(file);
                path_append(tmp_path, "firstboot.bin");
                if(storage_common_stat(
                       update_task->storage, furi_string_get_cstr(tmp_path), NULL) != FSE_OK) {
                    furi_string_set(tmp_path, update_task->update_path);
                    path_append(
                        tmp_path, furi_string_get_cstr(update_task->manifest->splash_file));
                }
            } else {
                path_append(tmp_path, furi_string_get_cstr(update_task->manifest->splash_file));
            }
            storage_common_remove(update_task->storage, SLIDESHOW_FS_PATH); // Overwrite if exists
            storage_common_copy(
                update_task->storage, furi_string_get_cstr(tmp_path), SLIDESHOW_FS_PATH);
            furi_string_free(tmp_path);
            update_task_set_progress(update_task, UpdateTaskStageSplashscreenInstall, 100);
        }
        success = true;
    } while(false);

    tar_archive_free(archive);
    furi_string_free(file_path);
    return success;
}

int32_t update_task_worker_backup_restore(void* context) {
    furi_assert(context);
    UpdateTask* update_task = context;

    FuriHalRtcBootMode boot_mode = update_task->boot_mode;
    if((boot_mode != FuriHalRtcBootModePreUpdate) && (boot_mode != FuriHalRtcBootModePostUpdate)) {
        /* no idea how we got here. Do nothing */
        return UPDATE_TASK_NOERR;
    }

    bool success = false;
    do {
        if(!update_task_parse_manifest(update_task)) {
            break;
        }

        if(boot_mode == FuriHalRtcBootModePreUpdate) {
            success = update_task_pre_update(update_task);
        } else if(boot_mode == FuriHalRtcBootModePostUpdate) { //-V547
            success = update_task_post_update(update_task);
            if(success) {
                update_operation_disarm();
            }
        }
    } while(false);

    if(!success) {
        update_task_set_progress(update_task, UpdateTaskStageError, 0);
        return UPDATE_TASK_FAILED;
    }

    update_task_set_progress(update_task, UpdateTaskStageCompleted, 100);
    return UPDATE_TASK_NOERR;
}
