#include "trampoline.h"

#include "lock.h"
#include "proc.h"

#include <assert.h>

// This is where sys_clone places new contexts that are created via clone.
_Thread_local struct LFIContext *new_ctx;

static struct LFIContext *
lfi_linux_clone_cb(struct LFIBox *box)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    assert(proc->libsyms.thread_create);

    // Invoke thread_create in clone_ctx and return the resulting new_ctx.
    LOCK_WITH_DEFER(&proc->lk_clone, lk_clone);
    LFI_INVOKE(box, &proc->clone_ctx, proc->libsyms.thread_create, void *,
        (void) );
    return new_ctx;
}

EXPORT void
lfi_linux_init_clone(struct LFILinuxThread *main)
{
    // Make sure the _lfi_thread_create symbol exists.
    assert(main->proc->libsyms.thread_create);

    // Set clone_ctx to main's ctx to indicate that we are fake-cloning from
    // main.
    main->proc->clone_ctx = main->ctx;
    // Invoke thread_create in main_thread.
    LFI_INVOKE(main->proc->box, &main->ctx, main->proc->libsyms.thread_create,
        void *, (void) );
    // Store the resulting new_ctx in clone_ctx to use for future clones.
    main->proc->clone_ctx = new_ctx;

    // Register lfi_linux_clone_cb as the clone_cb.
    lfi_set_clone_cb(lfi_box_engine(main->proc->box), lfi_linux_clone_cb);
}

static inline bool
bufcheck(struct LFIBox *box, lfiptr p, size_t size, size_t align)
{
    if (!lfi_box_ptrvalid(box, p))
        return false;
    if (!lfi_box_ptrvalid(box, p + size - 1))
        return false;
    if (p % align != 0)
        return false;
    return true;
}

EXPORT void *
lfi_lib_malloc(struct LFIContext **ctxp, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(lfi_ctx_box(*ctxp));
    assert(proc->libsyms.malloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.malloc, lfiptr,
        (size_t), size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void *
lfi_lib_realloc(struct LFIContext **ctxp, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(lfi_ctx_box(*ctxp));
    assert(proc->libsyms.realloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.realloc, lfiptr,
        (size_t), size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void *
lfi_lib_calloc(struct LFIContext **ctxp, size_t count, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(lfi_ctx_box(*ctxp));
    assert(proc->libsyms.calloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.calloc, lfiptr,
        (size_t, size_t), count, size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void
lfi_lib_free(struct LFIContext **ctxp, void *p)
{
    struct LFILinuxProc *proc = lfi_box_data(lfi_ctx_box(*ctxp));
    assert(proc->libsyms.free);

    LFI_INVOKE(proc->box, ctxp, proc->libsyms.free, void, (lfiptr),
        lfi_box_p2l(proc->box, (uintptr_t) p));
}
