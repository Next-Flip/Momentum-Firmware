# Minimal furi_hal implementation for ESP32-S3. See src/furi_hal_esp32s3.c
# and include/furi_hal.h for what this does and (extensively) does not cover
# yet - only the surface furi/core needs, plus a real furi_hal_gpio.
#
# This component is a dependency OF furi_core (furi_core REQUIRES
# momentum_hal), so it must not REQUIRE furi_core back - that would be a
# circular component dependency, which ESP-IDF's build doesn't support. A
# couple of source files here (memmgr_heap_stub.c's core/memmgr_heap.h,
# furi_hal_power.h's use of FURI_NORETURN from core/common_defines.h) still
# need furi/core's *headers*, so that path is added directly as a private
# include dir instead of going through the component system.

set(FURI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../furi")

idf_component_register(
    SRCS
        "src/furi_hal_esp32s3.c"
        "src/furi_hal_gpio_esp32s3.c"
        "src/memmgr_heap_stub.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${FURI_ROOT}"
    REQUIRES driver esp_rom esp_system
)
