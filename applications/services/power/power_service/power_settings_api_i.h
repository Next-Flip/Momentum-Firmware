#pragma once

#include "power.h"
#include "../power_settings.h"

void power_get_settings(Power* power, PowerSettings* settings);

void power_set_settings(Power* power, const PowerSettings* settings);
