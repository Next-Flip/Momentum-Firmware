#pragma once

#include "level99_can_types.h"

#include <storage/storage.h>

void level99_can_config_set_defaults(Level99CanConfig* config);
bool level99_can_config_is_valid(const Level99CanConfig* config);
bool level99_can_config_load(Storage* storage, Level99CanConfig* config);
bool level99_can_config_save(Storage* storage, const Level99CanConfig* config);
