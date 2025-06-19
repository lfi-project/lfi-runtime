#include "sys/sys.h"

#include <limits.h>
#include <stdatomic.h>

static void
clearctid(struct LFILinuxThread *t)
{
    _Atomic(int) *ctid;
    if (t->ctidp) {
        ctid = (_Atomic(int) *) ptrhost(t, t->ctidp);
        atomic_store_explicit(ctid, 0, memory_order_seq_cst);
    }
    sys_futex(t, t->ctidp, LINUX_FUTEX_WAKE, INT_MAX, 0, 0, 0);
}

uintptr_t
sys_exit(struct LFILinuxThread *t, int code)
{
    {
        LOCK_WITH_DEFER(&t->proc->lk_threads, lk_threads);
        list_remove(&t->proc->threads, &t->threads_elem);
    }
    clearctid(t);
    lfi_ctx_exit(t->ctx, code);
    assert(!"unreachable");
}
