#pragma once

#include <extra_beacon.h>

typedef struct {
    uint8_t mac[EXTRA_BEACON_MAC_ADDR_SIZE];
    uint8_t data[EXTRA_BEACON_MAX_DATA_SIZE];
    GapExtraBeaconConfig config;

    bool beacon_active;
    uint8_t broadcast_interval;
    uint8_t transmit_power;
} FindMyState;

bool findmy_state_load(FindMyState* out_state);

void findmy_state_apply(FindMyState* state);
