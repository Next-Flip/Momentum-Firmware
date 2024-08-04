#pragma once

#include <furi.h>
#include <input/input.h>

#include "desktop_settings.h"

#define RECORD_DESKTOP "desktop"

typedef struct Desktop Desktop;

typedef struct {
    bool locked;
} DesktopStatus;

bool desktop_api_is_locked(Desktop* instance);

void desktop_api_unlock(Desktop* instance);

FuriPubSub* desktop_api_get_status_pubsub(Desktop* instance);

void desktop_api_get_settings(Desktop* instance, DesktopSettings* settings);

void desktop_api_set_settings(Desktop* instance, const DesktopSettings* settings);

void desktop_run_keybind(Desktop* instance, InputType _type, InputKey _key);
