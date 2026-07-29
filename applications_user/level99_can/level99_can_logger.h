#pragma once

#include "level99_can_types.h"

#include <storage/storage.h>

typedef struct Level99CanLogger Level99CanLogger;

Level99CanLogger* level99_can_logger_alloc(Storage* storage);
void level99_can_logger_free(Level99CanLogger* logger);
bool level99_can_logger_start(Level99CanLogger* logger);
void level99_can_logger_stop(Level99CanLogger* logger);
bool level99_can_logger_enqueue(Level99CanLogger* logger, const Level99CanFrame* frame);
bool level99_can_logger_is_active(Level99CanLogger* logger);
uint32_t level99_can_logger_dropped(Level99CanLogger* logger);
const char* level99_can_logger_path(Level99CanLogger* logger);
