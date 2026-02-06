#include "lsm303.h"
#include <furi.h>
#include <furi_hal_rtc.h>

// I2C 7-bit addresses (shifted left for HAL API)
#define LSM303_ADDR_ACC (0x19 << 1)
#define LSM303_ADDR_MAG (0x1E << 1)

// LSM303AGR register map (see datasheet)
// Accelerometer: WHO_AM_I_A = 0x0F (0x33)
// Magnetometer: WHO_AM_I_M = 0x4F (0x40)

static bool lsm303_read_id(Lsm303* dev) {
    if(dev->mock) return true;
    uint8_t who_a = 0, who_m = 0;
    if(!i2c_read_reg(dev->bus, LSM303_ADDR_ACC, 0x0F, &who_a, 1)) {
        FURI_LOG_W("LSM303", "ACC WHOAMI read fail");
        return false;
    }
    (void)i2c_read_reg(dev->bus, LSM303_ADDR_MAG, 0x4F, &who_m, 1);
    FURI_LOG_I("LSM303", "WHO_A=0x%02X (exp 0x33), WHO_M=0x%02X (exp 0x40)", who_a, who_m);
    return true;
}

// Minimal init: enable accelerometer & magnetometer (AGR) in continuous mode
static bool lsm303_hw_init(Lsm303* dev) {
    if(dev->mock) return true;
    // Accelerometer: CTRL_REG1_A (0x20) -> ODR=100Hz (0101), LPen=0, Zen/Yen/Xen=1
    uint8_t ctrl1_a = 0x57; // 0101 0111
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_ACC, 0x20, &ctrl1_a, 1)) {
        FURI_LOG_W("LSM303", "ACC CTRL1 init fail");
        return false;
    }
    // Enable accelerometer internal temperature sensor: TEMP_CFG_REG_A (0x1F)
    // Set TEMP_EN1|TEMP_EN0 = 1 to enable sensor and ADC
    uint8_t temp_cfg_a = 0xC0; // 1100 0000 -> TEMP_EN = 11b
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_ACC, 0x1F, &temp_cfg_a, 1)) {
        FURI_LOG_W("LSM303", "ACC TEMP_CFG init fail");
        return false;
    }
    // CTRL4_A: BDU=1 (block data update), HR=1 (high-res), FS=00 (+/-2g)
    uint8_t ctrl4_a = 0x88; // 1000 1000 => BDU=1, BLE=0, FS=00, HR=1
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_ACC, 0x23, &ctrl4_a, 1)) {
        FURI_LOG_W("LSM303", "ACC CTRL4 init fail");
        return false;
    }

    // Magnetometer (AGR): configure via CFG_REG_A/B/C_M
    // CFG_REG_A_M (0x60): [b7 COMP_TEMP_EN]=1, [b4 LP]=0 (high-res), [b3:b2 ODR]=11 (100Hz), [b1:b0 MD]=00 (continuous)
    // Set to 100 Hz HR continuous for responsive heading
    uint8_t cfg_a_m = 0x8C; // 1000 1100 -> COMP_TEMP_EN=1, LP=0 (HR), ODR=100Hz, MD=00
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x60, &cfg_a_m, 1)) {
        FURI_LOG_W("LSM303", "MAG CFG_A init fail");
        return false;
    }
    // CFG_REG_B_M (0x61): enable offset cancellation (continuous); disable internal LPF for snappier response
    // Bit layout (7:5 reserved, 4: OFF_CANC_ONE_SHOT, 3: INT_on_DataOFF, 2: Set_FREQ, 1: OFF_CANC, 0: LPF)
    uint8_t cfg_b_m = 0x02; // OFF_CANC=1, LPF=0
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x61, &cfg_b_m, 1)) {
        FURI_LOG_W("LSM303", "MAG CFG_B init fail");
        return false;
    }
    // CFG_REG_C_M (0x62): BDU=1
    uint8_t cfg_c_m = 0x10; // BDU=1
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x62, &cfg_c_m, 1)) {
        FURI_LOG_W("LSM303", "MAG CFG_C init fail");
        return false;
    }
    furi_delay_ms(2);

    (void)lsm303_read_id(dev);
    return true;
}

void lsm303_init(Lsm303* dev, I2cBus* bus, bool mock) {
    dev->bus = bus;
    dev->mock = mock || bus->mock;
    dev->error_count = 0;
    dev->inited = lsm303_hw_init(dev);
}

static bool read_acc(Lsm303* dev, int16_t* ax, int16_t* ay, int16_t* az) {
    if(dev->mock) {
        *ax = 100; *ay = 0; *az = 16384; return true;
    }
    uint8_t reg = 0x28 | 0x80; // auto-increment from OUT_X_L_A
    uint8_t raw[6];
    if(!i2c_read_reg(dev->bus, LSM303_ADDR_ACC, reg, raw, 6)) { FURI_LOG_W("LSM303","ACC read fail"); return false; }
    // In HR mode, output is left-justified 12-bit. Cast first, then arithmetic shift by 4 to preserve sign.
    *ax = (int16_t)((raw[1] << 8) | raw[0]);
    *ay = (int16_t)((raw[3] << 8) | raw[2]);
    *az = (int16_t)((raw[5] << 8) | raw[4]);
    *ax >>= 4; *ay >>= 4; *az >>= 4;
    return true;
}

static bool read_mag(Lsm303* dev, int16_t* mx, int16_t* my, int16_t* mz) {
    if(dev->mock) { *mx = 0; *my = 0; *mz = 500; return true; }
    // LSM303AGR magnetic output registers start at 0x68: X_L, X_H, Y_L, Y_H, Z_L, Z_H
    // Set MSB of subaddress to enable auto-increment on I2C
    uint8_t raw[6];
    if(!i2c_read_reg(dev->bus, LSM303_ADDR_MAG, (uint8_t)(0x68 | 0x80), raw, 6)) {
        FURI_LOG_W("LSM303", "MAG read fail");
    // Try once to reconfigure and read again
    (void)lsm303_hw_init(dev);
    if(!i2c_read_reg(dev->bus, LSM303_ADDR_MAG, (uint8_t)(0x68 | 0x80), raw, 6)) return false;
    }
    *mx = (int16_t)((raw[1] << 8) | raw[0]);
    *my = (int16_t)((raw[3] << 8) | raw[2]);
    *mz = (int16_t)((raw[5] << 8) | raw[4]);
    return true;
}

static bool read_temp_degC(Lsm303* dev, float* temp_c_out) {
    if(dev->mock) { *temp_c_out = 30.0f; return true; }
    // OUT_TEMP_H_A is at 0x0D, OUT_TEMP_L_A at 0x0C (accelerometer bank)
    // Many ST MEMS map temperature as 8-bit in OUT_TEMP_H with 1 LSB/degC and 0 at 25°C
    uint8_t t_h = 0;
    if(!i2c_read_reg(dev->bus, LSM303_ADDR_ACC, 0x0D, &t_h, 1)) {
        FURI_LOG_W("LSM303", "TEMP read fail");
        return false;
    }
    int8_t t_off = (int8_t)t_h; // signed delta from 25°C
    *temp_c_out = 25.0f + (float)t_off;
    return true;
}

bool lsm303_poll(Lsm303* dev, Lsm303Sample* out) {
    out->ok = false;
    out->ts = furi_hal_rtc_get_timestamp();
    if(!dev->inited) {
        dev->inited = lsm303_hw_init(dev);
        if(!dev->inited) { dev->error_count++; return false; }
    }
    int16_t ax, ay, az, mx, my, mz;
    if(!read_acc(dev, &ax, &ay, &az)) { dev->error_count++; return false; }
    if(!read_mag(dev, &mx, &my, &mz)) { dev->error_count++; return false; }
    // Temperature (best-effort)
    float t_c = NAN;
    (void)read_temp_degC(dev, &t_c);
    // Convert accel: in HR 12-bit at +/-2g after >>4, 1 LSB = 1 mg. Convert to g.
    out->ax = (float)ax / 1000.0f;
    out->ay = (float)ay / 1000.0f;
    out->az = (float)az / 1000.0f;
    // LSM303AGR magnetometer sensitivity is typically 1.5 mGauss/LSB => 0.15 uT/LSB
    const float uT_per_lsb = 0.15f;
    out->mx = mx * uT_per_lsb;
    out->my = my * uT_per_lsb;
    out->mz = mz * uT_per_lsb;
    out->temp_c = t_c;
    out->ok = true;
    return true;
}

bool lsm303_set_hardiron_offsets_uT(Lsm303* dev, float off_x_uT, float off_y_uT, float off_z_uT) {
    if(!dev || !dev->inited) return false;
    if(dev->mock) return true;
    // Convert uT to LSB: 1 LSB = 0.15 uT
    int16_t ox = (int16_t)roundf(off_x_uT / 0.15f);
    int16_t oy = (int16_t)roundf(off_y_uT / 0.15f);
    int16_t oz = (int16_t)roundf(off_z_uT / 0.15f);
    uint8_t buf[2];
    // OFFSET_X_REG_L_M (0x45), H (0x46)
    buf[0] = (uint8_t)(ox & 0xFF); buf[1] = (uint8_t)((ox >> 8) & 0xFF);
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x45, buf, 2)) return false;
    // OFFSET_Y
    buf[0] = (uint8_t)(oy & 0xFF); buf[1] = (uint8_t)((oy >> 8) & 0xFF);
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x47, buf, 2)) return false;
    // OFFSET_Z
    buf[0] = (uint8_t)(oz & 0xFF); buf[1] = (uint8_t)((oz >> 8) & 0xFF);
    if(!i2c_write_reg(dev->bus, LSM303_ADDR_MAG, 0x49, buf, 2)) return false;
    return true;
}
