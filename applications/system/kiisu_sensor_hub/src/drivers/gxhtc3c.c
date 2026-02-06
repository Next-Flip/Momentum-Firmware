#include "gxhtc3c.h"
#include <furi.h>
#include <furi_hal_rtc.h>

// GXHTC3 I2C address 0x70 (7-bit)
#define SHTC3_ADDR (0x70 << 1)

// Optional: CRC-8 (poly 0x31) for data blocks; returns true if crc matches
static bool shtc3_check_crc(uint8_t msb, uint8_t lsb, uint8_t crc) {
    uint8_t data[2] = {msb, lsb};
    uint8_t c = 0xFF;
    for(size_t i = 0; i < 2; i++) {
        c ^= data[i];
        for(uint8_t b = 0; b < 8; b++) {
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
        }
    }
    return c == crc;
}

static bool shtc3_wake(I2cBus* bus) {
    // Wake command is 16-bit: 0x3517
    uint8_t cmd[2] = {0x35, 0x17};
    bool ok = i2c_write_bytes(bus, SHTC3_ADDR, cmd, 2);
    // Datasheet tSR ~180-500us; add small margin
    if(ok) furi_delay_ms(2);
    return ok;
}

static bool shtc3_sleep(I2cBus* bus) {
    // Sleep command 0xB098
    uint8_t cmd[2] = {0xB0, 0x98};
    return i2c_write_bytes(bus, SHTC3_ADDR, cmd, 2);
}

static bool shtc3_read_id(I2cBus* bus, uint16_t* id_out) {
    // Read ID command 0xEFC8, then read 2 bytes + CRC
    uint8_t cmd[2] = {0xEF, 0xC8};
    if(!i2c_write_bytes(bus, SHTC3_ADDR, cmd, 2)) return false;
    uint8_t r[3];
    if(!i2c_read_bytes(bus, SHTC3_ADDR, r, sizeof(r))) return false;
    if(!shtc3_check_crc(r[0], r[1], r[2])) {
        FURI_LOG_W("GXHTC3C", "ID CRC mismatch");
        return false;
    }
    *id_out = ((uint16_t)r[0] << 8) | r[1];
    return true;
}

static bool shtc3_measure(I2cBus* bus, uint8_t* data, size_t len) {
    // Use low-power, clock stretching ON, T first: 0x6458 (per datasheet Table 11)
    // With clock stretching, sensor ACKs the read header and holds SCL low until data ready
    uint8_t cmd_cs[2] = {0x64, 0x58};
    if(i2c_write_bytes(bus, SHTC3_ADDR, cmd_cs, 2)) {
        if(i2c_read_bytes(bus, SHTC3_ADDR, data, len)) return true;
    }
    // Fallback: low-power, T-first with clock stretching OFF (0x609C) + wait (~2ms)
    uint8_t cmd_ns[2] = {0x60, 0x9C};
    if(!i2c_write_bytes(bus, SHTC3_ADDR, cmd_ns, 2)) return false;
    furi_delay_ms(3);
    return i2c_read_bytes(bus, SHTC3_ADDR, data, len);
}

void gxhtc3c_init(Gxhtc3c* dev, I2cBus* bus, bool mock) {
    dev->bus = bus;
    dev->mock = mock || bus->mock;
    dev->inited = true;
    dev->error_count = 0;

    if(!dev->mock) {
        // Try wake + read ID once for debug
        if(shtc3_wake(bus)) {
            uint16_t id;
            if(shtc3_read_id(bus, &id)) {
                FURI_LOG_I("GXHTC3C", "Detected, ID=0x%04X", id);
            } else {
                dev->error_count++;
                FURI_LOG_W("GXHTC3C", "No ID resp (cnt=%lu)", dev->error_count);
            }
            shtc3_sleep(bus);
        } else {
            dev->error_count++;
            FURI_LOG_W("GXHTC3C", "Wake failed at init (cnt=%lu)", dev->error_count);
        }
    }
}

bool gxhtc3c_poll(Gxhtc3c* dev, Gxhtc3cSample* out) {
    out->ok = false;
    out->ts = furi_hal_rtc_get_timestamp();
    if(dev->mock) { out->temperature_c = 25.0f; out->humidity_rh = 45.0f; out->ok = true; return true; }
    if(!dev->inited) dev->inited = true;
    uint8_t raw[6];
    // wake and measure
    if(!shtc3_wake(dev->bus)) {
        dev->error_count++;
        FURI_LOG_W("GXHTC3C", "wake fail (%lu)", dev->error_count);
        return false;
    }
    if(!shtc3_measure(dev->bus, raw, 6)) {
        dev->error_count++;
        FURI_LOG_W("GXHTC3C", "measure fail (%lu)", dev->error_count);
        (void)shtc3_sleep(dev->bus);
        return false;
    }
    // Optional CRC check each block (temp and humidity)
    if(!shtc3_check_crc(raw[0], raw[1], raw[2]) || !shtc3_check_crc(raw[3], raw[4], raw[5])) {
        dev->error_count++;
        FURI_LOG_W("GXHTC3C", "data CRC fail (%lu)", dev->error_count);
        return false;
    }
    uint16_t t_raw = (uint16_t)((raw[0] << 8) | raw[1]);
    uint16_t h_raw = (uint16_t)((raw[3] << 8) | raw[4]);
    out->temperature_c = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    out->humidity_rh = 100.0f * ((float)h_raw / 65535.0f);
    out->ok = true;
    shtc3_sleep(dev->bus);
    return true;
}
