# Wraps furi/core (repo root) + furi/furi.c as an ESP-IDF component. This
# compiles the *actual* upstream sources (not a copy) so this port tracks
# furi/core as it evolves. See esp32s3_port/../PORTING_ESP32S3.md for the
# (small) set of #ifdef __arm__ / #ifdef __XTENSA__ branches added directly
# to furi/core/{check.h,check.c,common_defines.h,critical.c,memmgr.c} to
# make this possible - those files are shared with the STM32 build and were
# edited in place, not forked.
#
# Deliberately excluded: furi/core/memmgr_heap.c (STM32 SRAM1/SRAM2-specific
# allocator; ESP-IDF's own heap_caps-based allocator is used instead, see
# ../momentum_hal/src/memmgr_heap_stub.c for the small tracking-API shim
# that replaces it).

set(FURI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../furi")

set(FURI_CORE_SRCS
    "${FURI_ROOT}/furi.c"
    "${FURI_ROOT}/core/check.c"
    "${FURI_ROOT}/core/critical.c"
    "${FURI_ROOT}/core/event_flag.c"
    "${FURI_ROOT}/core/event_loop.c"
    "${FURI_ROOT}/core/event_loop_tick.c"
    "${FURI_ROOT}/core/event_loop_timer.c"
    "${FURI_ROOT}/core/kernel.c"
    "${FURI_ROOT}/core/log.c"
    "${FURI_ROOT}/core/memmgr.c"
    "${FURI_ROOT}/core/message_queue.c"
    "${FURI_ROOT}/core/mutex.c"
    "${FURI_ROOT}/core/pubsub.c"
    "${FURI_ROOT}/core/record.c"
    "${FURI_ROOT}/core/semaphore.c"
    "${FURI_ROOT}/core/stream_buffer.c"
    "${FURI_ROOT}/core/string.c"
    "${FURI_ROOT}/core/thread.c"
    "${FURI_ROOT}/core/thread_list.c"
    "${FURI_ROOT}/core/timer.c"
)

idf_component_register(
    SRCS ${FURI_CORE_SRCS}
    INCLUDE_DIRS "${FURI_ROOT}" "${FURI_ROOT}/.."
    REQUIRES momentum_hal freertos
)

# mlib (lib/mlib, vendored submodule) ships m-core.h etc. used by
# furi/core/check.h and furi/core/string.c. It's header-only, so it's added
# as an include path rather than a component.
target_include_directories(${COMPONENT_LIB} PUBLIC "${FURI_ROOT}/../lib/mlib")
