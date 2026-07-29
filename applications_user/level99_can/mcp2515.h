#pragma once

#include "level99_can_types.h"

#include <furi_hal.h>

typedef struct {
    const FuriHalSpiBusHandle* spi;
    bool initialized;
    uint32_t spi_errors;
} Mcp2515;

void mcp2515_init(Mcp2515* mcp, const FuriHalSpiBusHandle* spi);
void mcp2515_deinit(Mcp2515* mcp);
bool mcp2515_detect(Mcp2515* mcp);
bool mcp2515_reset(Mcp2515* mcp);
bool mcp2515_configure(
    Mcp2515* mcp,
    Level99CanOscillator oscillator,
    Level99CanBitrate bitrate,
    Level99CanMode mode);
bool mcp2515_set_mode(Mcp2515* mcp, Level99CanMode mode);
Level99CanMode mcp2515_get_mode(Mcp2515* mcp);
bool mcp2515_receive(Mcp2515* mcp, Level99CanFrame* frame);
bool mcp2515_transmit(Mcp2515* mcp, const Level99CanFrame* frame);
bool mcp2515_loopback_test(Mcp2515* mcp);
void mcp2515_read_diagnostics(Mcp2515* mcp, uint8_t* eflg, uint8_t* tec, uint8_t* rec);
