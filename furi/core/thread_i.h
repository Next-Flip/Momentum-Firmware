#pragma once

#include "thread.h"
#include "string.h"

#include <FreeRTOS.h>
#include <task.h>

typedef struct {
    FuriThreadStdoutWriteCallback write_callback;
    FuriString* buffer;
} FuriThreadStdout;

struct FuriThread {
    StaticTask_t container;
    StackType_t* stack_buffer;

    FuriThreadState state;
    int32_t ret;

    FuriThreadCallback callback;
    void* context;

    FuriThreadStateCallback state_callback;
    void* state_context;

    FuriThreadSignalCallback signal_callback;
    void* signal_context;

    char* name;
    char* appid;

    FuriThreadPriority priority;

    size_t stack_size;
    size_t heap_size;

    FuriThreadStdout output;

    // Keep all non-alignable byte types in one place,
    // this ensures that the size of this structure is minimal
    bool is_service;
    bool heap_trace_enabled;
    volatile bool is_active;
};

// IMPORTANT: container MUST be the FIRST struct member
static_assert(offsetof(FuriThread, container) == 0);
