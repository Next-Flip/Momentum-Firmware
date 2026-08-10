#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_memory_init(void);
void* furi_hal_memory_alloc(size_t size);
size_t furi_hal_memory_get_free(void);
size_t furi_hal_memory_max_pool_block(void);

#ifdef __cplusplus
}
#endif
