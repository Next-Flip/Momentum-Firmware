#pragma once
#include <furi.h>
#include "i2c_bus.h"

/* Hardware mapping
GXHTC3C (U1201): Pin1 VDD=+3V3; Pin3 SCL → I2C_SCL (PC0); Pin4 SDA → I2C_SDA (PC1); Pin6 GND=GND.
Notes: Both lines have ~51 Ω series resistors near sensors. Shares the same I²C bus as LSM303.
*/

typedef struct {
    I2cBus* bus;
    bool inited;
    bool mock;
    uint32_t error_count;
} Gxhtc3c;

typedef struct {
    float temperature_c;
    float humidity_rh;
    uint32_t ts;
    bool ok;
} Gxhtc3cSample;

void gxhtc3c_init(Gxhtc3c* dev, I2cBus* bus, bool mock);
bool gxhtc3c_poll(Gxhtc3c* dev, Gxhtc3cSample* out);
