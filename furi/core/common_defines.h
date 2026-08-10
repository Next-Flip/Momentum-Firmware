#pragma once

#include "core_defines.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define FURI_NORETURN [[noreturn]]
#else
#include <stdnoreturn.h>
#define FURI_NORETURN noreturn
#endif

#if defined(__arm__)
#include <cmsis_compiler.h>
#endif

#ifndef FURI_WARN_UNUSED
#define FURI_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#ifndef FURI_DEPRECATED
#define FURI_DEPRECATED __attribute__((deprecated))
#endif

#ifndef FURI_WEAK
#define FURI_WEAK __attribute__((weak))
#endif

#ifndef FURI_PACKED
#define FURI_PACKED __attribute__((packed))
#endif

#ifndef FURI_ALWAYS_INLINE
#define FURI_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#if defined(__arm__)

#ifndef FURI_IS_IRQ_MASKED
#define FURI_IS_IRQ_MASKED() (__get_PRIMASK() != 0U)
#endif

#ifndef FURI_IS_IRQ_MODE
#define FURI_IS_IRQ_MODE() (__get_IPSR() != 0U)
#endif

#ifndef FURI_DISABLE_IRQ
#define FURI_DISABLE_IRQ() __disable_irq()
#endif

#ifndef FURI_ENABLE_IRQ
#define FURI_ENABLE_IRQ() __enable_irq()
#endif

#elif defined(__XTENSA__)

// Xtensa (ESP32-S3) has no single PRIMASK-style bit and no IPSR register.
// "Masked" is approximated from the current PS.INTLEVEL: FreeRTOS's Xtensa
// port raises INTLEVEL to mask its managed interrupts inside a critical
// section (see portDISABLE_INTERRUPTS/portENTER_CRITICAL in ESP-IDF's
// freertos/portable), so a nonzero level here plays the same practical role
// PRIMASK!=0 plays on Cortex-M for furi's purposes (detecting "currently
// inside an IRQ-masked critical section, called from thread context").
// "In ISR" has a direct ESP-IDF equivalent: xPortInIsrContext(). Pull in
// FreeRTOS's own declaration for it (rather than hand-declaring an extern
// here) so its return type always matches whatever FreeRTOS.h/task.h
// declare later in the same translation unit - portmacro.h has its own
// include guard, so including it again there is a no-op, not a conflict.
#include "freertos/FreeRTOS.h"

static inline bool furi_hal_xtensa_is_irq_masked(void) {
    uint32_t ps;
    asm volatile("rsr.ps %0" : "=a"(ps));
    return (ps & 0xF) != 0U;
}

#ifndef FURI_IS_IRQ_MASKED
#define FURI_IS_IRQ_MASKED() furi_hal_xtensa_is_irq_masked()
#endif

#ifndef FURI_IS_IRQ_MODE
#define FURI_IS_IRQ_MODE() (xPortInIsrContext() != 0)
#endif

#ifndef FURI_DISABLE_IRQ
#define FURI_DISABLE_IRQ() portDISABLE_INTERRUPTS()
#endif

#ifndef FURI_ENABLE_IRQ
#define FURI_ENABLE_IRQ() portENABLE_INTERRUPTS()
#endif

#else
#error "furi/core: unsupported architecture, see common_defines.h"
#endif

#ifndef FURI_IS_ISR
#define FURI_IS_ISR() (FURI_IS_IRQ_MODE() || FURI_IS_IRQ_MASKED())
#endif

typedef struct {
    uint32_t isrm;
    bool from_isr;
    bool kernel_running;
} __FuriCriticalInfo;

__FuriCriticalInfo __furi_critical_enter(void);

void __furi_critical_exit(__FuriCriticalInfo info);

#ifndef FURI_CRITICAL_ENTER
#define FURI_CRITICAL_ENTER() __FuriCriticalInfo __furi_critical_info = __furi_critical_enter();
#endif

#ifndef FURI_CRITICAL_EXIT
#define FURI_CRITICAL_EXIT() __furi_critical_exit(__furi_critical_info);
#endif

#ifndef FURI_CHECK_RETURN
#define FURI_CHECK_RETURN __attribute__((__warn_unused_result__))
#endif

#ifndef FURI_NAKED
#define FURI_NAKED __attribute__((naked))
#endif

#ifndef FURI_DEFAULT
#define FURI_DEFAULT(x) __attribute__((weak, alias(x)))
#endif

#ifdef __cplusplus
}
#endif
