#pragma once

#include "power.h"
#include "power_settings.h"

// get settings from service to app
void power_api_get_settings(Power* instance, PowerSettings* settings);

// set settings from app to service
void power_api_set_settings(Power* instance, const PowerSettings* settings);
