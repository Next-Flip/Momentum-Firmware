#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LEVEL99_CAN_RING_CAPACITY          64U
#define LEVEL99_CAN_LOG_QUEUE_CAPACITY     32U
#define LEVEL99_CAN_COMMAND_QUEUE_CAPACITY 8U

typedef enum {
    Level99CanOscillator8MHz = 8,
    Level99CanOscillator16MHz = 16,
} Level99CanOscillator;

typedef enum {
    Level99CanBitrate10K = 10,
    Level99CanBitrate20K = 20,
    Level99CanBitrate50K = 50,
    Level99CanBitrate100K = 100,
    Level99CanBitrate125K = 125,
    Level99CanBitrate250K = 250,
    Level99CanBitrate500K = 500,
    Level99CanBitrate1000K = 1000,
} Level99CanBitrate;

typedef enum {
    Level99CanModeNormal,
    Level99CanModeSleep,
    Level99CanModeLoopback,
    Level99CanModeListenOnly,
    Level99CanModeConfiguration,
    Level99CanModeUnknown,
} Level99CanMode;

typedef enum {
    Level99CanIdAny,
    Level99CanIdStandard,
    Level99CanIdExtended,
} Level99CanIdFilter;

typedef enum {
    Level99CanFrameAny,
    Level99CanFrameData,
    Level99CanFrameRtr,
} Level99CanRtrFilter;

typedef struct {
    uint32_t timestamp_ms;
    uint32_t id;
    uint8_t data[8];
    uint8_t dlc;
    bool extended;
    bool rtr;
} Level99CanFrame;

typedef struct {
    uint32_t version;
    Level99CanOscillator oscillator;
    Level99CanBitrate bitrate;
    Level99CanMode default_mode;
    bool compact_display;
    bool logging_enabled;
    bool filter_enabled;
    uint32_t filter_id_min;
    uint32_t filter_id_max;
    Level99CanIdFilter id_filter;
    Level99CanRtrFilter rtr_filter;
} Level99CanConfig;

typedef struct {
    bool detected;
    bool spi_ok;
    bool interrupt_low;
    Level99CanMode mode;
    uint8_t eflg;
    uint8_t tec;
    uint8_t rec;
    uint32_t spi_errors;
    uint32_t received;
    uint32_t transmitted;
    uint32_t dropped;
    uint32_t log_dropped;
    uint32_t fps;
    char last_error[48];
} Level99CanDiagnostics;

typedef enum {
    Level99CanCommandReset,
    Level99CanCommandListenOnly,
    Level99CanCommandLoopbackTest,
    Level99CanCommandTransmit,
    Level99CanCommandApplyConfig,
    Level99CanCommandClearCounters,
    Level99CanCommandStop,
} Level99CanCommandType;

typedef struct {
    Level99CanCommandType type;
    Level99CanFrame frame;
} Level99CanCommand;
