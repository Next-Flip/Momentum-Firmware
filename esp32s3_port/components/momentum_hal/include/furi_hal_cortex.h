#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Name kept identical to the STM32 target's furi_hal_cortex_delay_us() even
// though this port doesn't run on a Cortex-M - furi/core calls this exact
// symbol (furi/core/kernel.c: furi_kernel_delay_us()) and changing the name
// would mean forking kernel.c too. Implemented with the Xtensa cycle
// counter (esp_cpu_get_cycle_count) in src/furi_hal_esp32s3.c.
void furi_hal_cortex_delay_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif
