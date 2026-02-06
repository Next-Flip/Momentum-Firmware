#pragma once
#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const FuriHalI2cBusHandle* handle;
    uint32_t timeout;
    bool mock;
} I2cBus;

void i2c_bus_init(I2cBus* bus, const FuriHalI2cBusHandle* handle, uint32_t timeout, bool mock);
bool i2c_read_reg(I2cBus* bus, uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
bool i2c_write_reg(I2cBus* bus, uint8_t addr, uint8_t reg, const uint8_t* data, size_t len);
bool i2c_read_bytes(I2cBus* bus, uint8_t addr, uint8_t* data, size_t len);
// Send raw bytes (no register address). Useful for devices that use 16-bit commands (e.g., SHTC3)
bool i2c_write_bytes(I2cBus* bus, uint8_t addr, const uint8_t* data, size_t len);
// Check if a device responds on the bus (addr is 8-bit, i.e., 7-bit<<1)
bool i2c_is_device_ready(I2cBus* bus, uint8_t addr, uint32_t timeout);

#ifdef __cplusplus
}
#endif
