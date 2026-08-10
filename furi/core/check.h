/**
 * @file check.h
 * 
 * Furi crash and assert functions.
 * 
 * The main problem with crashing is that you can't do anything without disturbing registers,
 * and if you disturb registers, you won't be able to see the correct register values in the debugger.
 * 
 * Current solution works around it by passing the message through r12 and doing some magic with registers in crash function.
 * r0-r10 are stored in the ram2 on crash routine start and restored at the end.
 * The only register that is going to be lost is r11.
 * 
 */
#pragma once

#include <m-core.h>
#include "common_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

// When enabled will use file paths instead of "furi_check failed"
#if !defined(FURI_RAM_EXEC) && !defined(FURI_DEBUG)
// #define __FURI_TRACE
#endif

// Flags instead of pointers will save ~4 bytes on furi_assert and furi_check calls.
#ifndef __FURI_TRACE
#define __FURI_ASSERT_MESSAGE_FLAG (0x01)
#define __FURI_CHECK_MESSAGE_FLAG  (0x02)
#else
#define __FURI_ASSERT_MESSAGE_FLAG __FILE__
#define __FURI_CHECK_MESSAGE_FLAG  __FILE__
#endif

// The r12-smuggling trick below only exists to preserve register state across
// the crash path on Cortex-M (see file header). It relies on ARM inline-asm
// register variables and does not apply to other architectures (e.g. Xtensa
// on the ESP32-S3 port), which get a plain function-call implementation
// instead - there's no register-preservation benefit to replicate there since
// nothing on that port inspects r0-r11 from a debugger the way OpenOCD/GDB
// does for the STM32 target.
#if defined(__arm__)

/** Crash system */
FURI_NORETURN void __furi_crash_implementation(void);

/** Halt system */
FURI_NORETURN void __furi_halt_implementation(void);

/** Crash system with message. Show message after reboot. */
#define __furi_crash(message)                                 \
    do {                                                      \
        register const void* r12 asm("r12") = (void*)message; \
        asm volatile("sukima%=:" : : "r"(r12));               \
        __furi_crash_implementation();                        \
    } while(0)

/** Halt system with message. */
#define __furi_halt(message)                                  \
    do {                                                      \
        register const void* r12 asm("r12") = (void*)message; \
        asm volatile("sukima%=:" : : "r"(r12));               \
        __furi_halt_implementation();                         \
    } while(0)

#else /* !__arm__ */

/** Crash system with message. Show message after reboot. */
FURI_NORETURN void __furi_crash_implementation(const void* message);

/** Halt system with message. */
FURI_NORETURN void __furi_halt_implementation(const void* message);

#define __furi_crash(message) __furi_crash_implementation((const void*)(message))

#define __furi_halt(message) __furi_halt_implementation((const void*)(message))

#endif /* __arm__ */

/** Crash system
 *
 * @param      ... optional  message (const char*)
 */
#define furi_crash(...) M_APPLY(__furi_crash, M_IF_EMPTY(__VA_ARGS__)((NULL), (__VA_ARGS__)))

/** Halt system
 *
 * @param      ... optional  message (const char*)
 */
#define furi_halt(...) M_APPLY(__furi_halt, M_IF_EMPTY(__VA_ARGS__)((NULL), (__VA_ARGS__)))

/** Check condition and crash if check failed */
#define __furi_check(__e, __m) \
    do {                       \
        if(!(__e)) {           \
            __furi_crash(__m); \
        }                      \
    } while(0)

/** Check condition and crash if failed
 *
 * @param      ... condition to check and optional  message (const char*)
 */
#define furi_check(...) \
    M_APPLY(__furi_check, M_DEFAULT_ARGS(2, (__FURI_CHECK_MESSAGE_FLAG), __VA_ARGS__))

/** Only in debug build: Assert condition and crash if assert failed  */
#ifdef FURI_DEBUG
#define __furi_assert(__e, __m) \
    do {                        \
        if(!(__e)) {            \
            __furi_crash(__m);  \
        }                       \
    } while(0)
#else
#define __furi_assert(__e, __m) \
    do {                        \
        ((void)(__e));          \
        ((void)(__m));          \
    } while(0)
#endif

/** Assert condition and crash if failed
 *
 * @warning    only will do check if firmware compiled in debug mode
 *
 * @param      ... condition to check and optional  message (const char*)
 */
#define furi_assert(...) \
    M_APPLY(__furi_assert, M_DEFAULT_ARGS(2, (__FURI_ASSERT_MESSAGE_FLAG), __VA_ARGS__))

#if defined(__arm__)
#define __furi_break_instruction() asm volatile("bkpt 0")
#else
#define __furi_break_instruction() __builtin_trap()
#endif

#define furi_break(__e)              \
    do {                             \
        if(!(__e)) {                 \
            __furi_break_instruction(); \
        }                            \
    } while(0)

#ifdef __cplusplus
}
#endif
