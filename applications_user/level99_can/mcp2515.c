#include "mcp2515.h"

#define TAG "L99MCP2515"

#define MCP_SPI_TIMEOUT_MS 20U

#define MCP_CMD_RESET      0xC0U
#define MCP_CMD_READ       0x03U
#define MCP_CMD_WRITE      0x02U
#define MCP_CMD_BIT_MODIFY 0x05U
#define MCP_CMD_RTS_TXB0   0x81U

#define MCP_REG_CANSTAT  0x0EU
#define MCP_REG_CANCTRL  0x0FU
#define MCP_REG_TEC      0x1CU
#define MCP_REG_REC      0x1DU
#define MCP_REG_CNF3     0x28U
#define MCP_REG_CNF2     0x29U
#define MCP_REG_CNF1     0x2AU
#define MCP_REG_CANINTE  0x2BU
#define MCP_REG_CANINTF  0x2CU
#define MCP_REG_EFLG     0x2DU
#define MCP_REG_TXB0CTRL 0x30U
#define MCP_REG_TXB0SIDH 0x31U
#define MCP_REG_RXB0CTRL 0x60U
#define MCP_REG_RXB0SIDH 0x61U
#define MCP_REG_RXB1CTRL 0x70U
#define MCP_REG_RXB1SIDH 0x71U

#define MCP_CANINTF_RX0IF 0x01U
#define MCP_CANINTF_RX1IF 0x02U
#define MCP_CANINTF_ERRIF 0x20U
#define MCP_EFLG_RX0OVR   0x40U
#define MCP_EFLG_RX1OVR   0x80U

#define MCP_MODE_MASK       0xE0U
#define MCP_MODE_NORMAL     0x00U
#define MCP_MODE_SLEEP      0x20U
#define MCP_MODE_LOOPBACK   0x40U
#define MCP_MODE_LISTENONLY 0x60U
#define MCP_MODE_CONFIG     0x80U

typedef struct {
    Level99CanOscillator oscillator;
    Level99CanBitrate bitrate;
    uint8_t cnf1;
    uint8_t cnf2;
    uint8_t cnf3;
} Mcp2515Timing;

/*
 * MCP2515 nominal bit timing derived from the Microchip MCP2515 bit-time
 * equations. Values are also consistent with the widely used MCP_CAN timing
 * tables. All requested 8/16 MHz Classic CAN combinations are represented.
 */
static const Mcp2515Timing mcp2515_timings[] = {
    {Level99CanOscillator8MHz, Level99CanBitrate10K, 0x0F, 0xBF, 0x87},
    {Level99CanOscillator8MHz, Level99CanBitrate20K, 0x07, 0xBF, 0x87},
    {Level99CanOscillator8MHz, Level99CanBitrate50K, 0x03, 0xB4, 0x86},
    {Level99CanOscillator8MHz, Level99CanBitrate100K, 0x01, 0xB4, 0x86},
    {Level99CanOscillator8MHz, Level99CanBitrate125K, 0x01, 0xB1, 0x85},
    {Level99CanOscillator8MHz, Level99CanBitrate250K, 0x00, 0xB1, 0x85},
    {Level99CanOscillator8MHz, Level99CanBitrate500K, 0x00, 0x90, 0x82},
    {Level99CanOscillator8MHz, Level99CanBitrate1000K, 0x00, 0x80, 0x80},
    {Level99CanOscillator16MHz, Level99CanBitrate10K, 0x31, 0xB8, 0x05},
    {Level99CanOscillator16MHz, Level99CanBitrate20K, 0x18, 0xB8, 0x05},
    {Level99CanOscillator16MHz, Level99CanBitrate50K, 0x09, 0xB8, 0x05},
    {Level99CanOscillator16MHz, Level99CanBitrate100K, 0x04, 0xB8, 0x05},
    {Level99CanOscillator16MHz, Level99CanBitrate125K, 0x03, 0xF0, 0x86},
    {Level99CanOscillator16MHz, Level99CanBitrate250K, 0x41, 0xF1, 0x85},
    {Level99CanOscillator16MHz, Level99CanBitrate500K, 0x00, 0xF0, 0x86},
    {Level99CanOscillator16MHz, Level99CanBitrate1000K, 0x00, 0xD0, 0x82},
};

static bool mcp2515_transaction(Mcp2515* mcp, const uint8_t* tx, uint8_t* rx, size_t size) {
    furi_hal_spi_acquire(mcp->spi);
    const bool ok = furi_hal_spi_bus_trx(mcp->spi, tx, rx, size, MCP_SPI_TIMEOUT_MS);
    furi_hal_spi_release(mcp->spi);
    if(!ok) mcp->spi_errors++;
    return ok;
}

static bool mcp2515_command(Mcp2515* mcp, uint8_t command) {
    return mcp2515_transaction(mcp, &command, NULL, 1U);
}

static bool mcp2515_read_registers(Mcp2515* mcp, uint8_t reg, uint8_t* data, size_t count) {
    if(count > 13U) return false;
    uint8_t tx[15] = {MCP_CMD_READ, reg};
    uint8_t rx[15] = {0};
    if(!mcp2515_transaction(mcp, tx, rx, count + 2U)) return false;
    memcpy(data, &rx[2], count);
    return true;
}

static uint8_t mcp2515_read_register(Mcp2515* mcp, uint8_t reg) {
    uint8_t value = 0xFFU;
    if(!mcp2515_read_registers(mcp, reg, &value, 1U)) return 0xFFU;
    return value;
}

static bool mcp2515_write_registers(Mcp2515* mcp, uint8_t reg, const uint8_t* data, size_t count) {
    if(count > 13U) return false;
    uint8_t tx[15] = {MCP_CMD_WRITE, reg};
    memcpy(&tx[2], data, count);
    return mcp2515_transaction(mcp, tx, NULL, count + 2U);
}

static bool mcp2515_write_register(Mcp2515* mcp, uint8_t reg, uint8_t value) {
    return mcp2515_write_registers(mcp, reg, &value, 1U);
}

static bool mcp2515_bit_modify(Mcp2515* mcp, uint8_t reg, uint8_t mask, uint8_t value) {
    const uint8_t tx[] = {MCP_CMD_BIT_MODIFY, reg, mask, value};
    return mcp2515_transaction(mcp, tx, NULL, sizeof(tx));
}

static uint8_t mcp2515_mode_bits(Level99CanMode mode) {
    switch(mode) {
    case Level99CanModeNormal:
        return MCP_MODE_NORMAL;
    case Level99CanModeSleep:
        return MCP_MODE_SLEEP;
    case Level99CanModeLoopback:
        return MCP_MODE_LOOPBACK;
    case Level99CanModeListenOnly:
        return MCP_MODE_LISTENONLY;
    case Level99CanModeConfiguration:
        return MCP_MODE_CONFIG;
    default:
        return 0xFFU;
    }
}

void mcp2515_init(Mcp2515* mcp, const FuriHalSpiBusHandle* spi) {
    furi_check(mcp);
    furi_check(spi);
    memset(mcp, 0, sizeof(*mcp));
    mcp->spi = spi;
    furi_hal_spi_bus_handle_init(spi);
    mcp->initialized = true;
}

void mcp2515_deinit(Mcp2515* mcp) {
    if(mcp && mcp->initialized) {
        furi_hal_spi_bus_handle_deinit(mcp->spi);
        mcp->initialized = false;
    }
}

bool mcp2515_reset(Mcp2515* mcp) {
    if(!mcp2515_command(mcp, MCP_CMD_RESET)) return false;
    furi_delay_ms(10U);
    return (mcp2515_read_register(mcp, MCP_REG_CANSTAT) & MCP_MODE_MASK) == MCP_MODE_CONFIG;
}

bool mcp2515_detect(Mcp2515* mcp) {
    if(!mcp2515_reset(mcp)) return false;
    const uint8_t canstat = mcp2515_read_register(mcp, MCP_REG_CANSTAT);
    const uint8_t canctrl = mcp2515_read_register(mcp, MCP_REG_CANCTRL);
    return canstat != 0xFFU && canctrl != 0xFFU && (canstat & MCP_MODE_MASK) == MCP_MODE_CONFIG;
}

bool mcp2515_set_mode(Mcp2515* mcp, Level99CanMode mode) {
    const uint8_t bits = mcp2515_mode_bits(mode);
    if(bits == 0xFFU) return false;
    if(!mcp2515_bit_modify(mcp, MCP_REG_CANCTRL, MCP_MODE_MASK, bits)) return false;
    for(uint8_t attempt = 0; attempt < 20U; attempt++) {
        if((mcp2515_read_register(mcp, MCP_REG_CANSTAT) & MCP_MODE_MASK) == bits) return true;
        furi_delay_ms(1U);
    }
    return false;
}

Level99CanMode mcp2515_get_mode(Mcp2515* mcp) {
    switch(mcp2515_read_register(mcp, MCP_REG_CANSTAT) & MCP_MODE_MASK) {
    case MCP_MODE_NORMAL:
        return Level99CanModeNormal;
    case MCP_MODE_SLEEP:
        return Level99CanModeSleep;
    case MCP_MODE_LOOPBACK:
        return Level99CanModeLoopback;
    case MCP_MODE_LISTENONLY:
        return Level99CanModeListenOnly;
    case MCP_MODE_CONFIG:
        return Level99CanModeConfiguration;
    default:
        return Level99CanModeUnknown;
    }
}

bool mcp2515_configure(
    Mcp2515* mcp,
    Level99CanOscillator oscillator,
    Level99CanBitrate bitrate,
    Level99CanMode mode) {
    const Mcp2515Timing* timing = NULL;
    for(size_t i = 0; i < COUNT_OF(mcp2515_timings); i++) {
        if(mcp2515_timings[i].oscillator == oscillator && mcp2515_timings[i].bitrate == bitrate) {
            timing = &mcp2515_timings[i];
            break;
        }
    }
    if(!timing || !mcp2515_set_mode(mcp, Level99CanModeConfiguration)) return false;

    const uint8_t timing_data[] = {timing->cnf3, timing->cnf2, timing->cnf1};
    if(!mcp2515_write_registers(mcp, MCP_REG_CNF3, timing_data, sizeof(timing_data))) return false;

    /* Accept all valid standard/extended frames; software filters are non-blocking. */
    if(!mcp2515_write_register(mcp, MCP_REG_RXB0CTRL, 0x64U) ||
       !mcp2515_write_register(mcp, MCP_REG_RXB1CTRL, 0x60U) ||
       !mcp2515_write_register(
           mcp, MCP_REG_CANINTE, MCP_CANINTF_RX0IF | MCP_CANINTF_RX1IF | MCP_CANINTF_ERRIF)) {
        return false;
    }
    mcp2515_bit_modify(mcp, MCP_REG_EFLG, MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR, 0U);
    return mcp2515_set_mode(mcp, mode);
}

static bool mcp2515_decode_frame(const uint8_t* raw, Level99CanFrame* frame) {
    const uint8_t sidh = raw[0];
    const uint8_t sidl = raw[1];
    const uint8_t eid8 = raw[2];
    const uint8_t eid0 = raw[3];
    frame->extended = (sidl & 0x08U) != 0U;
    if(frame->extended) {
        frame->id = ((uint32_t)sidh << 21U) | ((uint32_t)(sidl & 0xE0U) << 13U) |
                    ((uint32_t)(sidl & 0x03U) << 16U) | ((uint32_t)eid8 << 8U) | eid0;
    } else {
        frame->id = ((uint32_t)sidh << 3U) | (sidl >> 5U);
    }
    frame->rtr = (raw[4] & 0x40U) != 0U;
    frame->dlc = raw[4] & 0x0FU;
    if(frame->dlc > 8U) frame->dlc = 8U;
    memset(frame->data, 0, sizeof(frame->data));
    if(!frame->rtr) memcpy(frame->data, &raw[5], frame->dlc);
    return true;
}

bool mcp2515_receive(Mcp2515* mcp, Level99CanFrame* frame) {
    const uint8_t intf = mcp2515_read_register(mcp, MCP_REG_CANINTF);
    uint8_t flag;
    uint8_t address;
    if(intf & MCP_CANINTF_RX0IF) {
        flag = MCP_CANINTF_RX0IF;
        address = MCP_REG_RXB0SIDH;
    } else if(intf & MCP_CANINTF_RX1IF) {
        flag = MCP_CANINTF_RX1IF;
        address = MCP_REG_RXB1SIDH;
    } else {
        return false;
    }

    uint8_t raw[13];
    if(!mcp2515_read_registers(mcp, address, raw, sizeof(raw))) return false;
    if(!mcp2515_bit_modify(mcp, MCP_REG_CANINTF, flag, 0U)) return false;
    return mcp2515_decode_frame(raw, frame);
}

static bool mcp2515_validate_frame(const Level99CanFrame* frame) {
    if(!frame || frame->dlc > 8U) return false;
    return frame->extended ? frame->id <= 0x1FFFFFFFUL : frame->id <= 0x7FFUL;
}

bool mcp2515_transmit(Mcp2515* mcp, const Level99CanFrame* frame) {
    if(!mcp2515_validate_frame(frame)) return false;
    uint8_t raw[13] = {0};
    if(frame->extended) {
        raw[0] = (uint8_t)(frame->id >> 21U);
        raw[1] = (uint8_t)(((frame->id >> 13U) & 0xE0U) | 0x08U | ((frame->id >> 16U) & 0x03U));
        raw[2] = (uint8_t)(frame->id >> 8U);
        raw[3] = (uint8_t)frame->id;
    } else {
        raw[0] = (uint8_t)(frame->id >> 3U);
        raw[1] = (uint8_t)(frame->id << 5U);
    }
    raw[4] = frame->dlc | (frame->rtr ? 0x40U : 0U);
    if(!frame->rtr) memcpy(&raw[5], frame->data, frame->dlc);

    if(!mcp2515_write_registers(mcp, MCP_REG_TXB0SIDH, raw, 5U + frame->dlc)) return false;
    if(!mcp2515_bit_modify(mcp, MCP_REG_TXB0CTRL, 0x70U, 0U)) return false;
    if(!mcp2515_command(mcp, MCP_CMD_RTS_TXB0)) return false;

    /*
     * Do not switch the controller back to listen-only while TXREQ is still
     * pending. A bounded wait also gives the caller a deterministic failure
     * path when the bus is absent, disconnected, or reports a TX error.
     */
    for(uint16_t attempt = 0; attempt < 100U; attempt++) {
        const uint8_t tx_status = mcp2515_read_register(mcp, MCP_REG_TXB0CTRL);
        if(tx_status == 0xFFU) return false;
        if(!(tx_status & 0x08U)) return !(tx_status & 0x70U);
        furi_delay_ms(1U);
    }
    return false;
}

bool mcp2515_loopback_test(Mcp2515* mcp) {
    if(!mcp2515_set_mode(mcp, Level99CanModeLoopback)) return false;
    Level99CanFrame tx = {.id = 0x599U, .dlc = 2U, .data = {0x99U, 0x55U}};
    bool ok = mcp2515_transmit(mcp, &tx);
    Level99CanFrame rx;
    for(uint8_t i = 0; ok && i < 20U; i++) {
        if(mcp2515_receive(mcp, &rx)) {
            ok = rx.id == tx.id && rx.dlc == tx.dlc && rx.data[0] == tx.data[0] &&
                 rx.data[1] == tx.data[1];
            return ok;
        }
        furi_delay_ms(1U);
    }
    return false;
}

void mcp2515_read_diagnostics(Mcp2515* mcp, uint8_t* eflg, uint8_t* tec, uint8_t* rec) {
    if(eflg) *eflg = mcp2515_read_register(mcp, MCP_REG_EFLG);
    if(tec) *tec = mcp2515_read_register(mcp, MCP_REG_TEC);
    if(rec) *rec = mcp2515_read_register(mcp, MCP_REG_REC);
}
