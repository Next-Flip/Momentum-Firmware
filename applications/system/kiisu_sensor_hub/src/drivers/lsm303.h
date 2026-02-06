#pragma once
#include <furi.h>
#include "i2c_bus.h"

/* Hardware mapping
I²C bus (shared): I2C_SDA → MCU PA4, I2C_SCL → MCU PA7. Series resistors R1201/R1202 = 51 Ω in SDA/SCL near sensors. LSM303 & GXHTC3C both live on this bus.

LSM303AGRTR (U1204): Pin1 SCL → I2C_SCL (PA7); Pin4 SDA → I2C_SDA (PA4). CS pins present on symbol but not used; INT pins exist on symbol (Pin7 INT_MAG, Pin11 INT_2_XL, Pin12 INT_1_XL) not routed to MCU in this board revision → use polling by default. Power VDD=+3V3, GND=GND.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    I2cBus* bus;
    bool inited;
    bool mock;
    uint32_t error_count;
} Lsm303;

typedef struct {
    float ax, ay, az; // m/s^2 or g
    float mx, my, mz; // uT
    float temp_c;     // Accelerometer die temperature (approx, deg C)
    uint32_t ts;
    bool ok;
} Lsm303Sample;

void lsm303_init(Lsm303* dev, I2cBus* bus, bool mock);
bool lsm303_poll(Lsm303* dev, Lsm303Sample* out);

// Write hard-iron offsets (in microtesla) into sensor OFFSET registers (0x45-0x4A).
// Values are converted to LSB using 0.15 uT/LSB.
bool lsm303_set_hardiron_offsets_uT(Lsm303* dev, float off_x_uT, float off_y_uT, float off_z_uT);

#ifdef __cplusplus
}
#endif
