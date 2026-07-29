#include "level99_can_logger.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#define TAG                 "L99CanLog"
#define LEVEL99_CAN_LOG_DIR APP_DATA_PATH("logs")

struct Level99CanLogger {
    Storage* storage;
    File* file;
    FuriThread* thread;
    FuriMessageQueue* queue;
    FuriString* path;
    volatile bool active;
    volatile bool thread_started;
    volatile bool stop_requested;
    volatile bool write_failed;
    uint32_t dropped;
};

static bool level99_can_logger_unique_path(Level99CanLogger* logger) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    for(uint16_t suffix = 0; suffix < 1000U; suffix++) {
        if(suffix == 0U) {
            furi_string_printf(
                logger->path,
                LEVEL99_CAN_LOG_DIR "/level99_can_%04u%02u%02u_%02u%02u%02u.csv",
                now.year,
                now.month,
                now.day,
                now.hour,
                now.minute,
                now.second);
        } else {
            furi_string_printf(
                logger->path,
                LEVEL99_CAN_LOG_DIR "/level99_can_%04u%02u%02u_%02u%02u%02u_%03u.csv",
                now.year,
                now.month,
                now.day,
                now.hour,
                now.minute,
                now.second,
                suffix);
        }
        if(storage_common_stat(logger->storage, furi_string_get_cstr(logger->path), NULL) !=
           FSE_OK) {
            return true;
        }
    }
    return false;
}

static int32_t level99_can_logger_thread(void* context) {
    Level99CanLogger* logger = context;
    Level99CanFrame frame;
    char line[112];
    uint8_t pending_flush = 0U;

    while(!logger->stop_requested || furi_message_queue_get_count(logger->queue) > 0U) {
        if(furi_message_queue_get(logger->queue, &frame, 100U) != FuriStatusOk) continue;

        int pos = snprintf(
            line,
            sizeof(line),
            "%lu,0x%lX,%s,%u,%u,",
            (unsigned long)frame.timestamp_ms,
            (unsigned long)frame.id,
            frame.extended ? "extended" : "standard",
            frame.rtr ? 1U : 0U,
            frame.dlc);
        for(uint8_t i = 0; i < frame.dlc && pos > 0 && (size_t)pos < sizeof(line); i++) {
            pos += snprintf(
                &line[pos], sizeof(line) - (size_t)pos, "%s%02X", i ? " " : "", frame.data[i]);
        }
        if(pos > 0 && (size_t)pos < sizeof(line) - 1U) {
            line[pos++] = '\n';
            if(storage_file_write(logger->file, line, (size_t)pos) != (size_t)pos) {
                logger->write_failed = true;
                logger->active = false;
                break;
            }
        }

        if(++pending_flush >= 16U) {
            storage_file_sync(logger->file);
            pending_flush = 0U;
        }
    }
    storage_file_sync(logger->file);
    return 0;
}

Level99CanLogger* level99_can_logger_alloc(Storage* storage) {
    Level99CanLogger* logger = malloc(sizeof(*logger));
    if(!logger) return NULL;
    memset(logger, 0, sizeof(*logger));
    logger->storage = storage;
    logger->file = storage_file_alloc(storage);
    logger->queue =
        furi_message_queue_alloc(LEVEL99_CAN_LOG_QUEUE_CAPACITY, sizeof(Level99CanFrame));
    logger->path = furi_string_alloc();
    logger->thread =
        furi_thread_alloc_ex("L99CanLogger", 2048U, level99_can_logger_thread, logger);
    if(!logger->file || !logger->queue || !logger->path || !logger->thread) {
        level99_can_logger_free(logger);
        return NULL;
    }
    return logger;
}

void level99_can_logger_stop(Level99CanLogger* logger) {
    if(!logger || !logger->thread_started) return;
    logger->stop_requested = true;
    furi_thread_join(logger->thread);
    storage_file_close(logger->file);
    logger->active = false;
    logger->thread_started = false;
}

void level99_can_logger_free(Level99CanLogger* logger) {
    if(!logger) return;
    level99_can_logger_stop(logger);
    if(logger->thread) furi_thread_free(logger->thread);
    if(logger->queue) furi_message_queue_free(logger->queue);
    if(logger->file) storage_file_free(logger->file);
    if(logger->path) furi_string_free(logger->path);
    free(logger);
}

bool level99_can_logger_start(Level99CanLogger* logger) {
    if(!logger || logger->active) return logger && logger->active;
    if(logger->thread_started) level99_can_logger_stop(logger);
    storage_common_mkdir(logger->storage, STORAGE_APP_DATA_PATH_PREFIX);
    storage_common_mkdir(logger->storage, LEVEL99_CAN_LOG_DIR);
    if(!level99_can_logger_unique_path(logger) ||
       !storage_file_open(
           logger->file, furi_string_get_cstr(logger->path), FSAM_WRITE, FSOM_CREATE_NEW)) {
        return false;
    }
    static const char header[] = "timestamp_ms,id,id_type,rtr,dlc,data\n";
    if(storage_file_write(logger->file, header, sizeof(header) - 1U) != sizeof(header) - 1U) {
        storage_file_close(logger->file);
        return false;
    }
    logger->stop_requested = false;
    logger->write_failed = false;
    logger->dropped = 0U;
    logger->active = true;
    logger->thread_started = true;
    furi_thread_start(logger->thread);
    return true;
}

bool level99_can_logger_enqueue(Level99CanLogger* logger, const Level99CanFrame* frame) {
    if(!logger || !logger->active || logger->write_failed) return false;
    if(furi_message_queue_put(logger->queue, frame, 0U) != FuriStatusOk) {
        logger->dropped++;
        return false;
    }
    return true;
}

bool level99_can_logger_is_active(Level99CanLogger* logger) {
    return logger && logger->active && !logger->write_failed;
}

uint32_t level99_can_logger_dropped(Level99CanLogger* logger) {
    return logger ? logger->dropped : 0U;
}

const char* level99_can_logger_path(Level99CanLogger* logger) {
    return logger ? furi_string_get_cstr(logger->path) : "";
}
