#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"
#include "log.h"
#include "proc.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#define ensure(expr)                                                   \
    do {                                                               \
        if (!(expr))                                                   \
            ERROR("%s:%d: ensure failed: " #expr, __FILE__, __LINE__); \
    } while (0)

struct AttachedCtx {
    struct LFILinuxProc *proc;
    struct LFIContext *ctx;
    struct AttachedCtx *next;
};

static void thread_destructor(void *p);

static bool
attach_ctx(struct LFILinuxProc *proc, struct LFIContext *ctx)
{
    struct AttachedCtx *node = malloc(sizeof(*node));
    if (!node) {
        ERROR("%s:%d: failed to allocate AttachedCtx", __FILE__, __LINE__);
        return false;
    }
    node->proc = proc;
    node->ctx = ctx;
    node->next = lfi_thread_cleanup_data();
    if (!lfi_set_thread_cleanup(thread_destructor, node)) {
        ERROR("%s:%d: failed to register thread cleanup", __FILE__, __LINE__);
        free(node);
        return false;
    }
    return true;
}

// Tear down a single (proc, ctx) attachment. Used by both thread_destructor
// (on host thread exit) and by lfi_linux_detach_thread.
//
// The runtime built the context entirely host-side, so lfi_thread_free is the
// whole teardown.
//
// If this detach takes attached_threads to zero and lfi_proc_free has
// already been called (pending_free), finish the deferred free here. While
// attached_threads is non-zero, lfi_proc_free is required to defer the
// actual free, so the proc memory we touch above is guaranteed live.
static void
detach_ctx(struct LFILinuxProc *proc, struct LFIContext *ctx)
{
    struct LFILinuxThread *thread = lfi_ctx_data(ctx);

    lfi_thread_free(thread);

    lock(&proc->lk_proc);
    int remaining = atomic_fetch_sub_explicit(&proc->attached_threads, 1,
        memory_order_relaxed) - 1;
    bool do_destroy = (remaining == 0 && proc->pending_free);
    unlock(&proc->lk_proc);

    if (do_destroy)
        proc_destroy(proc);
}

static void
thread_destructor(void *p)
{
    struct AttachedCtx *node = p;
    while (node) {
        struct AttachedCtx *next = node->next;
        detach_ctx(node->proc, node->ctx);
        free(node);
        node = next;
    }
}

// Clone callback: allocates a stack and creates a context for a host thread
// that is entering the sandbox for the first time. No pthread or TLS
// involvement.
static struct LFIContext *
lfi_linux_clone_cb(struct LFIBox *box)
{
    struct LFILinuxProc *proc = lfi_box_data(box);

    // Create thread structure. This runs on a host thread the embedder owns,
    // so it is an LFI_THREAD_HOST: it is reclaimed when that host thread exits
    // (or detaches), and exit_group must never signal or wait on it.
    struct LFILinuxThread *t = thread_alloc(proc, LFI_THREAD_HOST);
    if (!t)
        return NULL;

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
#if defined(LFI_ARCH_ARM64) || defined(LFI_ARCH_RISCV64)
    lfi_ctx_regs(t->ctx)->sp = t->stack + stacksize;
#elif defined(LFI_ARCH_X64)
    lfi_ctx_regs(t->ctx)->rsp = t->stack + stacksize;
#else
#error "invalid arch"
#endif

    // Set tp to 0 (no TLS support).
    lfi_ctx_set_tp(t->ctx, 0);

    // Hand ownership of the thread to the host thread that is attaching, so
    // that its stack is reclaimed when that thread exits rather than being
    // held until the proc is destroyed. attached_threads keeps lfi_proc_free
    // from freeing the proc out from under a still-attached thread.
    atomic_fetch_add_explicit(&proc->attached_threads, 1,
        memory_order_relaxed);
    if (!attach_ctx(proc, t->ctx)) {
        atomic_fetch_sub_explicit(&proc->attached_threads, 1,
            memory_order_relaxed);
        lfi_thread_free(t);
        return NULL;
    }

    return t->ctx;
}

EXPORT void
lfi_linux_init_clone(struct LFILinuxThread *main)
{
    lfi_set_clone_cb(lfi_box_engine(main->proc->box), lfi_linux_clone_cb);
}

EXPORT void
lfi_linux_detach_thread(struct LFILinuxProc *proc)
{
    struct AttachedCtx *head = lfi_thread_cleanup_data();
    struct AttachedCtx **link = &head;
    while (*link) {
        if ((*link)->proc == proc) {
            struct AttachedCtx *node = *link;
            *link = node->next;
            lfi_set_thread_cleanup(thread_destructor, head);
            struct LFIContext *ctx = node->ctx;
            free(node);
            detach_ctx(proc, ctx);
            return;
        }
        link = &(*link)->next;
    }
}

static inline bool
bufcheck(struct LFIBox *box, lfiptr p, size_t size, size_t align)
{
    if (!lfi_box_bufvalid(box, p, size))
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
lfi_lib_realloc(struct LFIBox *box, struct LFIContext **ctxp, void *old, size_t size)
{
    struct LFILinuxProc *proc = lfi_box_data(box);
    ensure(proc->libsyms.realloc);

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.realloc, lfiptr,
        (lfiptr, size_t), lfi_box_p2l(proc->box, (uintptr_t) old), size);
    if (!bufcheck(proc->box, p, size, 16)) {
        LOG(proc->engine, "sandbox realloc returned invalid pointer: %lx", p);
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

    size_t total;
    if (__builtin_mul_overflow(count, size, &total))
        return NULL;

    lfiptr p = LFI_INVOKE(proc->box, ctxp, proc->libsyms.calloc, lfiptr,
        (size_t, size_t), count, size);
    if (!bufcheck(proc->box, p, total, 16)) {
        LOG(proc->engine, "sandbox calloc returned invalid pointer: %lx", p);
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
