#include "list.h"
#include "sys/sys.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>

static void
clearctid(struct LFILinuxThread *t)
{
#ifndef SYS_MINIMAL
    _Atomic(int) *ctid;
    if (t->ctidp) {
        ctid = (_Atomic(int) *) ptrhost(t, t->ctidp);
        atomic_store_explicit(ctid, 0, memory_order_seq_cst);
        sys_futex(t, t->ctidp, LINUX_FUTEX_WAKE, INT_MAX, 0, 0, 0);
    }
#else
    (void) t;
#endif
}

extern void
lfi_ret_end(struct LFIContext *ctx) __asm__("lfi_ret_end");

void
thread_exit(struct LFILinuxThread *t, enum LFIExitKind kind, int code)
{
#ifdef SYS_MINIMAL
    if (t->owner != LFI_THREAD_MAIN) {
        ERROR("sandbox tried to exit a context that was never entered to run");
        abort();
    }
#endif

    // Publish the request before anything else, so a thread that reaches a
    // checkpoint while we are still unwinding sees it.
    if (kind == LFI_EXIT_PROCESS) {
        atomic_store_explicit(&t->proc->exit_code, code, memory_order_relaxed);
        atomic_store_explicit(&t->proc->terminating, true,
            memory_order_release);
    }

#ifndef SYS_MINIMAL
    // A host thread that attached itself through the trampoline is not ours to
    // terminate.
    if (t->owner == LFI_THREAD_HOST) {
        clearctid(t);
        lfi_ret_end(t->ctx);
        __builtin_unreachable();
    }
#endif

    if (t->paused) {
        ERROR("sandbox tried to exit a context that has already paused");
        abort();
    }

    // Already exited.
    if (atomic_exchange_explicit(&t->exited, true, memory_order_acq_rel))
        abort();

    {
        LOCK_WITH_DEFER(&t->proc->lk_threads, lk_threads);
        list_remove(&t->proc->threads, &t->threads_elem);
    }
    clearctid(t);

    lfi_ctx_exit(t->ctx, code);
    __builtin_unreachable();
}

uintptr_t
sys_exit(struct LFILinuxThread *t, int code)
{
    thread_exit(t, LFI_EXIT_THREAD, code);
}
