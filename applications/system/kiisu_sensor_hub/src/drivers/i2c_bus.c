#include "i2c_bus.h"
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <stm32wbxx_ll_gpio.h>

#define I2C_RETRIES 3

void i2c_bus_init(I2cBus* bus, const FuriHalI2cBusHandle* handle, uint32_t timeout, bool mock) {
    bus->handle = handle;
    bus->timeout = timeout;
    bus->mock = mock;
}

static bool i2c_tx_once(I2cBus* bus, uint8_t addr, const uint8_t* tx, size_t len) {
    if(bus->mock) return true;
    for(int i = 0; i < I2C_RETRIES; i++) {
        furi_hal_i2c_acquire(bus->handle);
        // Make sure external I2C pins have pull-ups enabled (PC1=SDA, PC0=SCL)
        if(bus->handle == &furi_hal_i2c_handle_external) {
            LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
            LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
        }
        bool ok = furi_hal_i2c_tx(bus->handle, addr, tx, len, bus->timeout);
        furi_hal_i2c_release(bus->handle);
        if(ok) return true;
        furi_delay_ms(2);
    }
    return false;
}

static bool i2c_rx_once(I2cBus* bus, uint8_t addr, uint8_t* rx, size_t len) {
    if(bus->mock) {
        memset(rx, 0, len);
        return true;
    }
    for(int i = 0; i < I2C_RETRIES; i++) {
        furi_hal_i2c_acquire(bus->handle);
        if(bus->handle == &furi_hal_i2c_handle_external) {
            LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
            LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
        }
        bool ok = furi_hal_i2c_rx(bus->handle, addr, rx, len, bus->timeout);
        furi_hal_i2c_release(bus->handle);
        if(ok) return true;
        furi_delay_ms(2);
    }
    return false;
}

bool i2c_read_reg(I2cBus* bus, uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    if(bus->mock) {
        memset(data, 0, len);
        return true;
    }
    bool ok = false;
    for(int i = 0; i < I2C_RETRIES; i++) {
        furi_hal_i2c_acquire(bus->handle);
        if(bus->handle == &furi_hal_i2c_handle_external) {
            LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
            LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
        }
        ok = furi_hal_i2c_read_mem(bus->handle, addr, reg, data, len, bus->timeout);
        furi_hal_i2c_release(bus->handle);
        if(ok) break;
        furi_delay_ms(2);
    }
    return ok;
}

bool i2c_write_reg(I2cBus* bus, uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) {
    if(bus->mock) return true;
    bool ok = false;
    for(int i = 0; i < I2C_RETRIES; i++) {
        furi_hal_i2c_acquire(bus->handle);
        if(bus->handle == &furi_hal_i2c_handle_external) {
            LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
            LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
        }
        ok = furi_hal_i2c_write_mem(bus->handle, addr, reg, (uint8_t*)data, len, bus->timeout);
        furi_hal_i2c_release(bus->handle);
        if(ok) break;
        furi_delay_ms(2);
    }
    return ok;
}

bool i2c_read_bytes(I2cBus* bus, uint8_t addr, uint8_t* data, size_t len) {
    return i2c_rx_once(bus, addr, data, len);
}

bool i2c_write_bytes(I2cBus* bus, uint8_t addr, const uint8_t* data, size_t len) {
    return i2c_tx_once(bus, addr, data, len);
}

bool i2c_is_device_ready(I2cBus* bus, uint8_t addr, uint32_t timeout) {
    if(bus->mock) return true;
    bool ready = false;
    for(int i = 0; i < I2C_RETRIES; i++) {
        furi_hal_i2c_acquire(bus->handle);
        if(bus->handle == &furi_hal_i2c_handle_external) {
            LL_GPIO_SetPinPull(gpio_ext_pc1.port, gpio_ext_pc1.pin, LL_GPIO_PULL_UP);
            LL_GPIO_SetPinPull(gpio_ext_pc0.port, gpio_ext_pc0.pin, LL_GPIO_PULL_UP);
        }
        ready = furi_hal_i2c_is_device_ready(bus->handle, addr, timeout);
        furi_hal_i2c_release(bus->handle);
        if(ready) break;
        furi_delay_ms(2);
    }
    return ready;
}
