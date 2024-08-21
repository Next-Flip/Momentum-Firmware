#include "run_parallel.h"

#include <core/timer.h>
#include <stdlib.h>

static void run_parallel_pending_callback(void* context, uint32_t arg) {
    UNUSED(arg);

    FuriThread* thread = context;
    furi_thread_join(thread);
    furi_thread_free(thread);
}

static void run_parallel_thread_state(FuriThreadState state, void* context) {
    UNUSED(context);

    if(state == FuriThreadStateStopped) {
        furi_timer_pending_callback(run_parallel_pending_callback, furi_thread_get_current(), 0);
    }
}

void run_parallel(FuriThreadCallback callback, void* context, uint32_t stack_size) {
    FuriThread* thread = furi_thread_alloc_ex(NULL, stack_size, callback, context);
    furi_thread_set_state_callback(thread, run_parallel_thread_state);
    furi_thread_start(thread);
}
