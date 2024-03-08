#include "findmy_state.h"

#include <string.h>
#include <stddef.h>
#include <furi_hal_bt.h>

bool findmy_state_load(FindMyState* out_state) {
    FindMyState state;

    // Set default values
    state.beacon_active = false;
    state.broadcast_interval = 5;
    state.transmit_power = 6;
    state.config.min_adv_interval_ms = state.broadcast_interval * 1000; // Converting s to ms
    state.config.max_adv_interval_ms = (state.broadcast_interval * 1000) + 150;
    state.config.adv_channel_map = GapAdvChannelMapAll;
    state.config.adv_power_level = GapAdvPowerLevel_0dBm + state.transmit_power;
    state.config.address_type = GapAddressTypePublic;

    // Set default mac
    uint8_t default_mac[EXTRA_BEACON_MAC_ADDR_SIZE] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    memcpy(state.mac, default_mac, sizeof(state.mac));
    memcpy(state.config.address, default_mac, sizeof(state.config.address));

    // Set default empty AirTag data
    uint8_t* data = state.data;
    *data++ = 0x1E; // Length
    *data++ = 0xFF; // Manufacturer Specific Data
    *data++ = 0x4C; // Company ID (Apple, Inc.)
    *data++ = 0x00; // ...
    *data++ = 0x12; // Type (FindMy)
    *data++ = 0x19; // Length
    *data++ = 0x00; // Status
    // Placeholder Empty Public Key without the MAC address
    for(size_t i = 0; i < 22; ++i) {
        *data++ = 0x00;
    }
    *data++ = 0x00; // First 2 bits are the version, the rest is the battery level
    *data++ = 0x00; // Hint (0x00)

    // Copy to caller state before popping stack
    memcpy(out_state, &state, sizeof(state));

    // Return if active, can be used to start after loading in an if statement
    return state.beacon_active;
}

void findmy_state_apply(FindMyState* state) {
    // Stop any running beacon
    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_check(furi_hal_bt_extra_beacon_stop());
    }

    furi_check(furi_hal_bt_extra_beacon_set_config(&state->config));

    furi_check(furi_hal_bt_extra_beacon_set_data(state->data, sizeof(state->data)));

    if(state->beacon_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }
}
