/**
 * Minimal furi_hal implementation backing furi/core on ESP32-S3.
 *
 * This is deliberately small: it only implements the handful of furi_hal_*
 * symbols furi/core's kernel/thread/memory/check code calls directly (see
 * PORTING_ESP32S3.md, "Phase 1"). It is NOT a replacement for the STM32
 * target's furi_hal - all of radio, NFC, IR, BLE, storage, GPIO-resource
 * management, power management, crypto, etc. still need their own
 * furi_hal_* implementations before the rest of the firmware (applications/)
 * can be brought up.
 */
#include "furi_hal_cortex.h"
#include "furi_hal_memory.h"
#include "furi_hal_power.h"
#include "furi_hal_rtc.h"

#include "esp_rom_sys.h"
#include "esp_system.h"

void furi_hal_cortex_delay_us(uint32_t microseconds) {
    // esp_rom_delay_us busy-waits using the CPU cycle counter and is safe to
    // call before the scheduler starts, which furi_kernel_delay_us() (the
    // only caller) relies on for very short (<1 tick) delays.
    esp_rom_delay_us(microseconds);
}

// The STM32 target has a small dedicated backup-SRAM pool that
// furi_hal_memory_alloc() serves allocations from without ever freeing them
// (used for allocations that must outlive furi_hal_memory being unusable
// during a crash dump). ESP32-S3 has no equivalent reserved region here; we
// report an empty pool so memmgr_alloc_from_pool() in furi/core/memmgr.c
// falls through to its normal malloc() path, which is the documented
// fallback for pool exhaustion.
void furi_hal_memory_init(void) {
}

void* furi_hal_memory_alloc(size_t size) {
    (void)size;
    return NULL;
}

size_t furi_hal_memory_get_free(void) {
    return 0;
}

size_t furi_hal_memory_max_pool_block(void) {
    return 0;
}

FURI_NORETURN void furi_hal_power_reset(void) {
    esp_restart();
}

FuriHalRtcHeapTrackMode furi_hal_rtc_get_heap_track_mode(void) {
    // Allocation-tracking-by-thread is a debug feature (see
    // furi/core/thread.c). Off by default until furi_hal_rtc is properly
    // ported (it currently just persists this in STM32 backup registers).
    return FuriHalRtcHeapTrackModeNone;
}
