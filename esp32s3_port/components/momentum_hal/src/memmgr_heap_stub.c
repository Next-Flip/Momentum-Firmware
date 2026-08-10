/**
 * Stand-in for furi/core/memmgr_heap.c, which is a heavily modified
 * FreeRTOS heap_4 allocator that bakes in STM32 SRAM1/SRAM2 region layout
 * (stm32wb55_linker.h) to provide per-thread allocation tracking. ESP-IDF
 * ships its own heap allocator (multi-region: internal RAM + octal PSRAM,
 * capability-based via heap_caps) which furi/core/memmgr.c already calls
 * through the standard pvPortMalloc/vPortFree FreeRTOS port API, so that
 * part needs no replacement.
 *
 * Per-thread allocation tracking (used by the debug "Threads" screen to
 * show each thread's heap usage) has no equivalent yet - these are honest
 * no-ops, not a claim that tracking works. Wiring this up for real would
 * mean either patching ESP-IDF's heap component to tag allocations by
 * task (like memmgr_heap.c does for FreeRTOS heap_4), or building a
 * separate shadow-allocator, which is out of scope for the Phase 1 bring-up
 * this component supports.
 */
#include "core/memmgr_heap.h"

void memmgr_heap_enable_thread_trace(FuriThreadId thread_id) {
    (void)thread_id;
}

void memmgr_heap_disable_thread_trace(FuriThreadId thread_id) {
    (void)thread_id;
}

size_t memmgr_heap_get_thread_memory(FuriThreadId thread_id) {
    (void)thread_id;
    return MEMMGR_HEAP_UNKNOWN;
}

size_t memmgr_heap_get_max_free_block(void) {
    return 0;
}

void memmgr_heap_printf_free_blocks(void) {
}
