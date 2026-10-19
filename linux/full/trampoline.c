#include "elfsym.h"
#include "full.h"
#include "lock.h"

_Thread_local struct LFIContext *full_new_ctx;

void
full_init_clone(struct LFILinuxThread *main)
{
    struct LFILinuxProc *proc = main->proc;
    struct LFIFullProc *full = proc->full;

    if (!elf_loadsym(proc, &full->thread_create, "_lfi_thread_create") ||
        !elf_loadsym(proc, &full->thread_destroy, "_lfi_thread_destroy")) {
        LOG(proc->engine,
            "_lfi_thread_create/_lfi_thread_destroy not found: attached host threads will not have TLS");
        full->thread_create = 0;
        full->thread_destroy = 0;
        return;
    }

    LOCK_WITH_DEFER(&full->lk_clone, lk_clone);

    // Set clone_ctx to main's ctx to indicate that we are fake-cloning from
    // main.
    full->clone_ctx = main->ctx;
    full_new_ctx = NULL;
    // Invoke thread_create in main_thread.
    LFI_INVOKE(proc->box, &main->ctx, full->thread_create, void *, (void) );
    // Store the resulting new_ctx in clone_ctx to use for future clones. If
    // the clone failed, clone_ctx is left NULL and the runtime builds the
    // contexts for host threads itself.
    full->clone_ctx = full_new_ctx;
    if (!full->clone_ctx)
        LOG(proc->engine,
            "_lfi_thread_create did not clone a thread: attached host threads will not have TLS");
}

bool
full_clone_thread(struct LFILinuxProc *proc, struct LFILinuxThread **o_t)
{
    struct LFIFullProc *full = proc->full;

    {
        // Read clone_ctx under the lock too, since full_init_clone may still
        // be setting it up.
        LOCK_WITH_DEFER(&full->lk_clone, lk_clone);
        if (!full->clone_ctx)
            return false;
        // Reset new_ctx.
        full_new_ctx = NULL;
        (void) LFI_INVOKE(proc->box, &full->clone_ctx, full->thread_create,
            lfiptr, (void) );
    }

    // Cloning failed for some reason.
    if (!full_new_ctx) {
        *o_t = NULL;
        return true;
    }

    // sys_clone already marked this LFI_THREAD_HOST on the way through.
    struct LFILinuxThread *t = lfi_ctx_data(full_new_ctx);
    t->full->sandbox_attached = true;
    *o_t = t;
    return true;
}

void
full_detach_thread(struct LFILinuxThread *t)
{
    // The sandbox allocated this thread's stack and libc state inside
    // _lfi_thread_create, so it has to tear them down itself. The thread
    // leaves the sandbox through exit, which for a host thread returns from
    // this invocation (see full_thread_exit).
    if (!t->full->sandbox_attached)
        return;

    // The thread_destroy arg is currently unused.
    LFI_INVOKE(t->proc->box, &t->ctx, t->proc->full->thread_destroy, void,
        (lfiptr), (lfiptr) 0);
}
