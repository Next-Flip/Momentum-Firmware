#include "level99_can_worker.h"

#include "mcp2515.h"

#include <furi.h>
#include <furi_hal.h>

#define TAG "L99CanWorker"

struct Level99CanWorker {
    Level99CanConfig config;
    Level99CanLogger* logger;
    Mcp2515 mcp;
    FuriThread* thread;
    FuriMessageQueue* commands;
    FuriMutex* mutex;
    Level99CanFrame ring[LEVEL99_CAN_RING_CAPACITY];
    size_t ring_head;
    size_t ring_count;
    Level99CanDiagnostics diagnostics;
    volatile bool running;
    volatile bool capture;
    volatile bool paused;
};

static void level99_can_worker_error(Level99CanWorker* worker, const char* error) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    strlcpy(worker->diagnostics.last_error, error, sizeof(worker->diagnostics.last_error));
    furi_mutex_release(worker->mutex);
}

static bool
    level99_can_worker_filter(const Level99CanConfig* config, const Level99CanFrame* frame) {
    if(!config->filter_enabled) return true;
    if(frame->id < config->filter_id_min || frame->id > config->filter_id_max) return false;
    if(config->id_filter == Level99CanIdStandard && frame->extended) return false;
    if(config->id_filter == Level99CanIdExtended && !frame->extended) return false;
    if(config->rtr_filter == Level99CanFrameData && frame->rtr) return false;
    if(config->rtr_filter == Level99CanFrameRtr && !frame->rtr) return false;
    return true;
}

static void level99_can_worker_push(Level99CanWorker* worker, const Level99CanFrame* frame) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->diagnostics.received++;
    if(!worker->paused && level99_can_worker_filter(&worker->config, frame)) {
        if(worker->ring_count == LEVEL99_CAN_RING_CAPACITY) {
            worker->diagnostics.dropped++;
        } else {
            worker->ring_count++;
        }
        worker->ring[worker->ring_head] = *frame;
        worker->ring_head = (worker->ring_head + 1U) % LEVEL99_CAN_RING_CAPACITY;
    }
    furi_mutex_release(worker->mutex);
    if(level99_can_logger_is_active(worker->logger)) {
        level99_can_logger_enqueue(worker->logger, frame);
    }
}

static void level99_can_worker_apply_config(Level99CanWorker* worker) {
    if(!mcp2515_configure(
           &worker->mcp,
           worker->config.oscillator,
           worker->config.bitrate,
           worker->config.default_mode)) {
        level99_can_worker_error(worker, "Controller configuration failed");
    }
    worker->diagnostics.mode = mcp2515_get_mode(&worker->mcp);
}

static void
    level99_can_worker_handle_command(Level99CanWorker* worker, const Level99CanCommand* command) {
    bool ok = false;
    switch(command->type) {
    case Level99CanCommandReset:
        ok = mcp2515_detect(&worker->mcp);
        worker->diagnostics.detected = ok;
        worker->diagnostics.spi_ok = ok;
        if(ok) {
            level99_can_worker_apply_config(worker);
            level99_can_worker_error(worker, "Controller reset");
        }
        break;
    case Level99CanCommandListenOnly:
        ok = mcp2515_set_mode(&worker->mcp, Level99CanModeListenOnly);
        break;
    case Level99CanCommandLoopbackTest:
        ok = mcp2515_loopback_test(&worker->mcp);
        mcp2515_configure(
            &worker->mcp,
            worker->config.oscillator,
            worker->config.bitrate,
            Level99CanModeListenOnly);
        level99_can_worker_error(worker, ok ? "Loopback test passed" : "Loopback test failed");
        break;
    case Level99CanCommandTransmit:
        /*
         * Active mode is entered only for this confirmed one-shot request,
         * then immediately returned to listen-only regardless of outcome.
         */
        ok = mcp2515_set_mode(&worker->mcp, Level99CanModeNormal) &&
             mcp2515_transmit(&worker->mcp, &command->frame);
        mcp2515_set_mode(&worker->mcp, Level99CanModeListenOnly);
        if(ok) worker->diagnostics.transmitted++;
        break;
    case Level99CanCommandApplyConfig:
        level99_can_worker_apply_config(worker);
        ok = true;
        break;
    case Level99CanCommandClearCounters:
        furi_mutex_acquire(worker->mutex, FuriWaitForever);
        worker->diagnostics.received = 0U;
        worker->diagnostics.transmitted = 0U;
        worker->diagnostics.dropped = 0U;
        worker->diagnostics.log_dropped = 0U;
        worker->diagnostics.spi_errors = 0U;
        worker->mcp.spi_errors = 0U;
        furi_mutex_release(worker->mutex);
        ok = true;
        break;
    case Level99CanCommandStop:
        worker->running = false;
        ok = true;
        break;
    }
    if(!ok && command->type != Level99CanCommandLoopbackTest) {
        level99_can_worker_error(worker, "Controller command failed");
    }
    worker->diagnostics.mode = mcp2515_get_mode(&worker->mcp);
}

static int32_t level99_can_worker_thread(void* context) {
    Level99CanWorker* worker = context;
    mcp2515_init(&worker->mcp, &furi_hal_spi_bus_handle_external);
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeInput, GpioPullUp, GpioSpeedLow);

    worker->diagnostics.spi_ok = true;
    worker->diagnostics.detected = mcp2515_detect(&worker->mcp);
    if(worker->diagnostics.detected) {
        level99_can_worker_apply_config(worker);
        strlcpy(
            worker->diagnostics.last_error, "No error", sizeof(worker->diagnostics.last_error));
    } else {
        worker->diagnostics.spi_ok = false;
        level99_can_worker_error(worker, "MCP2515 not detected");
    }

    uint32_t fps_start = furi_get_tick();
    uint32_t fps_frames = 0U;
    uint32_t diagnostics_tick = 0U;
    while(worker->running) {
        Level99CanCommand command;
        while(furi_message_queue_get(worker->commands, &command, 0U) == FuriStatusOk) {
            if(worker->diagnostics.detected || command.type == Level99CanCommandReset ||
               command.type == Level99CanCommandClearCounters ||
               command.type == Level99CanCommandStop) {
                level99_can_worker_handle_command(worker, &command);
            }
        }

        if(worker->capture && worker->diagnostics.detected) {
            Level99CanFrame frame;
            uint8_t drained = 0U;
            while(drained < 8U && mcp2515_receive(&worker->mcp, &frame)) {
                frame.timestamp_ms = furi_get_tick();
                level99_can_worker_push(worker, &frame);
                fps_frames++;
                drained++;
            }
        }

        const uint32_t now = furi_get_tick();
        if(now - fps_start >= 1000U) {
            furi_mutex_acquire(worker->mutex, FuriWaitForever);
            worker->diagnostics.fps = fps_frames;
            worker->diagnostics.log_dropped = level99_can_logger_dropped(worker->logger);
            furi_mutex_release(worker->mutex);
            fps_frames = 0U;
            fps_start = now;
        }
        if(worker->diagnostics.detected && now - diagnostics_tick >= 250U) {
            uint8_t eflg;
            uint8_t tec;
            uint8_t rec;
            mcp2515_read_diagnostics(&worker->mcp, &eflg, &tec, &rec);
            furi_mutex_acquire(worker->mutex, FuriWaitForever);
            worker->diagnostics.interrupt_low = !furi_hal_gpio_read(&gpio_ext_pb2);
            worker->diagnostics.eflg = eflg;
            worker->diagnostics.tec = tec;
            worker->diagnostics.rec = rec;
            worker->diagnostics.spi_errors = worker->mcp.spi_errors;
            worker->diagnostics.mode = mcp2515_get_mode(&worker->mcp);
            if(worker->diagnostics.mode == Level99CanModeUnknown) {
                worker->diagnostics.detected = false;
                worker->diagnostics.spi_ok = false;
                strlcpy(
                    worker->diagnostics.last_error,
                    "Controller disconnected",
                    sizeof(worker->diagnostics.last_error));
            }
            furi_mutex_release(worker->mutex);
            diagnostics_tick = now;
        }
        furi_delay_ms(2U);
    }

    if(worker->diagnostics.detected) {
        mcp2515_set_mode(&worker->mcp, Level99CanModeListenOnly);
    }
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    mcp2515_deinit(&worker->mcp);
    return 0;
}

Level99CanWorker*
    level99_can_worker_alloc(const Level99CanConfig* config, Level99CanLogger* logger) {
    Level99CanWorker* worker = malloc(sizeof(*worker));
    if(!worker) return NULL;
    memset(worker, 0, sizeof(*worker));
    worker->config = *config;
    worker->logger = logger;
    worker->commands =
        furi_message_queue_alloc(LEVEL99_CAN_COMMAND_QUEUE_CAPACITY, sizeof(Level99CanCommand));
    worker->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->thread = furi_thread_alloc_ex("L99CanRx", 3072U, level99_can_worker_thread, worker);
    if(!worker->commands || !worker->mutex || !worker->thread) {
        level99_can_worker_free(worker);
        return NULL;
    }
    return worker;
}

void level99_can_worker_stop(Level99CanWorker* worker) {
    if(!worker || !worker->running) return;
    Level99CanCommand command = {.type = Level99CanCommandStop};
    if(!level99_can_worker_command(worker, &command)) worker->running = false;
    furi_thread_join(worker->thread);
}

void level99_can_worker_free(Level99CanWorker* worker) {
    if(!worker) return;
    level99_can_worker_stop(worker);
    if(worker->thread) furi_thread_free(worker->thread);
    if(worker->mutex) furi_mutex_free(worker->mutex);
    if(worker->commands) furi_message_queue_free(worker->commands);
    free(worker);
}

bool level99_can_worker_start(Level99CanWorker* worker) {
    if(!worker || worker->running) return false;
    worker->running = true;
    worker->capture = true;
    furi_thread_start(worker->thread);
    return true;
}

bool level99_can_worker_command(Level99CanWorker* worker, const Level99CanCommand* command) {
    return worker && command &&
           furi_message_queue_put(worker->commands, command, 20U) == FuriStatusOk;
}

void level99_can_worker_set_capture(Level99CanWorker* worker, bool enabled) {
    if(worker) worker->capture = enabled;
}

void level99_can_worker_set_paused(Level99CanWorker* worker, bool paused) {
    if(worker) worker->paused = paused;
}

void level99_can_worker_update_config(Level99CanWorker* worker, const Level99CanConfig* config) {
    if(!worker || !config) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->config = *config;
    furi_mutex_release(worker->mutex);
}

void level99_can_worker_clear(Level99CanWorker* worker) {
    if(!worker) return;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->ring_count = 0U;
    worker->ring_head = 0U;
    furi_mutex_release(worker->mutex);
}

size_t level99_can_worker_snapshot(
    Level99CanWorker* worker,
    Level99CanFrame* frames,
    size_t capacity,
    Level99CanDiagnostics* diagnostics) {
    if(!worker) return 0U;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    size_t count = MIN(worker->ring_count, capacity);
    size_t start =
        (worker->ring_head + LEVEL99_CAN_RING_CAPACITY - count) % LEVEL99_CAN_RING_CAPACITY;
    for(size_t i = 0; i < count; i++) {
        frames[i] = worker->ring[(start + i) % LEVEL99_CAN_RING_CAPACITY];
    }
    if(diagnostics) *diagnostics = worker->diagnostics;
    furi_mutex_release(worker->mutex);
    return count;
}

bool level99_can_worker_latest(Level99CanWorker* worker, Level99CanFrame* frame) {
    if(!worker || !frame) return false;
    bool result = false;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    if(worker->ring_count) {
        *frame =
            worker->ring
                [(worker->ring_head + LEVEL99_CAN_RING_CAPACITY - 1U) % LEVEL99_CAN_RING_CAPACITY];
        result = true;
    }
    furi_mutex_release(worker->mutex);
    return result;
}
