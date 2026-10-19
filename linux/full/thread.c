#include "full.h"
#include "full/sys/sys.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>

// First TID, to avoid using low TID numbers.
#define BASE_TID 10000

static int
next_tid(struct LFILinuxProc *p)
{
    return BASE_TID +
        atomic_fetch_add_explicit(&p->full->total_thread_count, 1,
            memory_order_relaxed);
}

bool
full_thread_init(struct LFILinuxThread *t)
{
    struct LFIFullThread *full = calloc(1, sizeof(*full));
    if (!full)
        return false;
    full->tid = next_tid(t->proc);
    t->full = full;
    return true;
}

void
full_thread_free(struct LFILinuxThread *t)
{
    free(t->full);
    t->full = NULL;
}

struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t, enum LFIThreadOwner owner)
{
    struct LFILinuxThread *new_t = thread_alloc(t->proc, owner);
    if (!new_t)
        return NULL;
    struct LFIContext *ctx = lfi_ctx_new(t->proc->box, new_t);
    if (!ctx) {
        thread_dealloc(new_t);
        return NULL;
    }
    new_t->ctx = ctx;
    // Copy all registers.
    *lfi_ctx_regs(new_t->ctx) = *lfi_ctx_regs(t->ctx);
    lfi_ctx_regs_relink_ctxreg(new_t->ctx);

    return new_t;
}

static void
clearctid(struct LFILinuxThread *t)
{
    lfiptr ctidp = t->full->ctidp;
    if (!ctidp)
        return;
    // Like Linux, a write to an address the sandbox has since unmapped is
    // skipped. The futex wake below only needs the address.
    int zero = 0;
    lfi_box_copyto_checked(t->proc->box, ctidp, &zero, sizeof(zero));
    full_sys_futex(t, ctidp, LINUX_FUTEX_WAKE, INT_MAX, 0, 0, 0);
}

extern void
lfi_ret_end(struct LFIContext *ctx) __asm__("lfi_ret_end");

void
full_thread_exit(struct LFILinuxThread *t)
{
    clearctid(t);

    // A host thread that attached itself through the trampoline is not ours to
    // terminate: return from the invocation it is in instead.
    if (t->owner == LFI_THREAD_HOST) {
        lfi_ret_end(t->ctx);
        __builtin_unreachable();
    }
}
