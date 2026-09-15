#include "sys/sys.h"

#include <stdatomic.h>
#include <stdlib.h>

void
thread_exit(struct LFILinuxThread *t, enum LFIExitKind kind, int code)
{
    if (t->owner != LFI_THREAD_MAIN) {
        ERROR("sandbox tried to exit a context that was never entered to run");
        abort();
    }

    // Publish the request before anything else, so a thread that reaches a
    // checkpoint while we are still unwinding sees it.
    if (kind == LFI_EXIT_PROCESS) {
        atomic_store_explicit(&t->proc->exit_code, code, memory_order_relaxed);
        atomic_store_explicit(&t->proc->terminating, true,
            memory_order_release);
    }

    if (t->paused) {
        ERROR("sandbox tried to exit a context that has already paused");
        abort();
    }

    // Already exited.
    if (atomic_exchange_explicit(&t->exited, true, memory_order_acq_rel))
        abort();

    lfi_ctx_exit(t->ctx, code);
    __builtin_unreachable();
}

uintptr_t
sys_exit(struct LFILinuxThread *t, int code)
{
    thread_exit(t, LFI_EXIT_THREAD, code);
}
