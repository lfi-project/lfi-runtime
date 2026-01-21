#include "sys/sys.h"

#include <limits.h>
#include <stdatomic.h>

static void
clearctid(struct LFILinuxThread *t)
{
#ifndef SYS_MINIMAL
    _Atomic(int) *ctid;
    if (t->ctidp) {
        ctid = (_Atomic(int) *) ptrhost(t, t->ctidp);
        atomic_store_explicit(ctid, 0, memory_order_seq_cst);
    }
    sys_futex(t, t->ctidp, LINUX_FUTEX_WAKE, INT_MAX, 0, 0, 0);
#else
    (void) t;
#endif
}

extern void
lfi_ret_end(struct LFIContext *ctx) __asm__("lfi_ret_end");

uintptr_t
sys_exit(struct LFILinuxThread *t, int code)
{
    // If the current sandbox thread was created by the lazily clone mechanism,
    // then it is exiting due to a destructor invocation of
    // _lfi_thread_destroy. We should service that by simulating the return of
    // that function via lfi_ret.
    if (t->box_pthread) {
        clearctid(t);
        lfi_ret_end(t->ctx);
        __builtin_unreachable();
    }
    {
        LOCK_WITH_DEFER(&t->proc->lk_threads, lk_threads);
        list_remove(&t->proc->threads, &t->threads_elem);
    }
    clearctid(t);
    lfi_ctx_exit(t->ctx, code);
    __builtin_unreachable();
}
