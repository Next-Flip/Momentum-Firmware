#pragma once

#include "level99_can_logger.h"
#include "level99_can_types.h"

typedef struct Level99CanWorker Level99CanWorker;

Level99CanWorker*
    level99_can_worker_alloc(const Level99CanConfig* config, Level99CanLogger* logger);
void level99_can_worker_free(Level99CanWorker* worker);
bool level99_can_worker_start(Level99CanWorker* worker);
void level99_can_worker_stop(Level99CanWorker* worker);
bool level99_can_worker_command(Level99CanWorker* worker, const Level99CanCommand* command);
void level99_can_worker_set_capture(Level99CanWorker* worker, bool enabled);
void level99_can_worker_set_paused(Level99CanWorker* worker, bool paused);
void level99_can_worker_update_config(Level99CanWorker* worker, const Level99CanConfig* config);
void level99_can_worker_clear(Level99CanWorker* worker);
size_t level99_can_worker_snapshot(
    Level99CanWorker* worker,
    Level99CanFrame* frames,
    size_t capacity,
    Level99CanDiagnostics* diagnostics);
bool level99_can_worker_latest(Level99CanWorker* worker, Level99CanFrame* frame);
