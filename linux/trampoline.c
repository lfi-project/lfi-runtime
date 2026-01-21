#include "trampoline.h"

#include "lfi_arch.h"
#include "lfi_core.h"
#include "lock.h"
#include "proc.h"

#include <assert.h>
#include <stdlib.h>

#define ensure(expr)                                                   \
    do {                                                               \
        if (!(expr))                                                   \
            ERROR("%s:%d: ensure failed: " #expr, __FILE__, __LINE__); \
    } while (0)

#ifdef SYS_MINIMAL
// Simplified clone callback for sys_minimal mode.
// Just allocates a stack and creates a context - no pthread involvement.
static struct LFIContext *
lfi_linux_clone_cb_minimal(struct LFIBox *box)
{
    struct LFILinuxProc *proc = lfi_box_data(box);

    // Create thread structure.
    struct LFILinuxThread *t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!t)
        return NULL;

    t->proc = proc;
    t->ctx = lfi_ctx_new(box, t);
    if (!t->ctx) {
        free(t);
        return NULL;
    }

    // Allocate stack in sandbox memory.
    size_t stacksize = proc->engine->opts.stacksize;
    t->stack = lfi_box_mapany(box, stacksize,
        LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (t->stack == (lfiptr) -1) {
        lfi_ctx_free(t->ctx);
        free(t);
        return NULL;
    }
    t->stack_size = stacksize;

    // Set stack pointer to top of stack (stack grows down).
    lfi_ctx_regs(t->ctx)->sp = t->stack + stacksize;

    // Set tp to 0 (no TLS support in sys_minimal mode).
    lfi_ctx_set_tp(t->ctx, 0);

    return t->ctx;
}
#endif

#if defined(SANDBOX_TLS) && !defined(SYS_MINIMAL)
// This is where sys_clone places new contexts that are created via clone.
thread_local struct LFIContext *new_ctx;

// Pthread key used for destroying sandbox threads.
static pthread_key_t thread_key;

static struct LFIContext *
lfi_linux_clone_cb(struct LFIBox *box)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.thread_create);

    // Invoke thread_create in clone_ctx and return the resulting new_ctx.
    LOCK_WITH_DEFER(&proc->lk_clone, lk_clone);
    lfiptr pt = LFI_INVOKE(box, &proc->clone_ctx, proc->libsyms.thread_create,
        lfiptr, (void) );

    struct LFILinuxThread *thread = lfi_ctx_data(new_ctx);
    thread->box_pthread = pt;
    pthread_setspecific(thread_key, new_ctx);

    return new_ctx;
}

// Called when a host thread that has an associated sandbox thread exits.
static void
thread_destructor(void *p)
{
    (void) p;

    struct LFIContext *ctx = p;
    struct LFILinuxThread *thread = lfi_ctx_data(ctx);

    struct LFILinuxProc *proc = thread->proc;

    ensure(proc->libsyms.thread_destroy);

    LFI_INVOKE(proc->box, &ctx, proc->libsyms.thread_destroy, void, (lfiptr),
        thread->box_pthread);

    LOCK_WITH_DEFER(&proc->lk_clone, lk_clone);
    LFI_INVOKE(proc->box, &proc->clone_ctx, proc->libsyms.free, void, (lfiptr),
        thread->box_pthread);

    lfi_thread_free(thread);
}
#endif

EXPORT void
lfi_linux_init_clone(struct LFILinuxThread *main)
{
#ifdef SYS_MINIMAL
    // Simple clone callback - just allocates stack, no pthread/TLS.
    lfi_set_clone_cb(lfi_box_engine(main->proc->box), lfi_linux_clone_cb_minimal);
#elif defined(SANDBOX_TLS)
    // Make sure the _lfi_thread_create symbol exists.
    ensure(main->proc->libsyms.thread_create);

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

    // Initialize our pthread key so that we get a callback when host threads
    // that have associated sandbox threads exit.
    pthread_key_create(&thread_key, thread_destructor);
#else
    (void) main;
#endif
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
lfi_lib_malloc(struct LFIBox *box, struct LFIContext **ctxp, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.malloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.malloc, lfiptr,
        (size_t), size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void *
lfi_lib_realloc(struct LFIBox *box, struct LFIContext **ctxp, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.realloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.realloc, lfiptr,
        (size_t), size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void *
lfi_lib_calloc(struct LFIBox *box, struct LFIContext **ctxp, size_t count,
    size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.calloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.calloc, lfiptr,
        (size_t, size_t), count, size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox malloc returned invalid pointer: %lx", p);
        return NULL;
    }
    return (void *) lfi_box_l2p(proc->box, p);
}

EXPORT void
lfi_lib_free(struct LFIBox *box, struct LFIContext **ctxp, void *p)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.free);

    LFI_INVOKE(proc->box, ctxp, proc->libsyms.free, void, (lfiptr),
        lfi_box_p2l(proc->box, (uintptr_t) p));
}

EXPORT void
lfi_lib_setjmp(struct LFIBox *box, struct LFIContext **ctxp, void *env, void *host_env, void (*callback)(void *, int))
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.setjmp);

    LFI_INVOKE(proc->box, ctxp, proc->libsyms.setjmp, int, (lfiptr, lfiptr, lfiptr),
        lfi_box_p2l(proc->box, (uintptr_t) env),
        lfi_box_p2l(proc->box, (uintptr_t) host_env),
        lfi_box_p2l(proc->box, (uintptr_t) callback));
}
