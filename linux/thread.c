#include "lfi_arch.h"
#include "linux.h"
#include "proc.h"

#include <stdatomic.h>
#include <stdlib.h>

// First TID, to avoid using low TID numbers.
#define BASE_TID 10000

static int
next_tid(struct LFILinuxProc *p)
{
    return BASE_TID +
        atomic_fetch_add_explicit(&p->threads, 1, memory_order_relaxed);
}

EXPORT struct LFILinuxThread *
lfi_thread_new(struct LFILinuxProc *proc, int argc, char **argv, char **envp)
{
    struct LFILinuxThread *t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!t)
        goto err1;
    t->proc = proc;
    t->ctx = lfi_ctx_new(proc->box, t);
    if (!t->ctx)
        goto err2;
    t->tid = next_tid(proc);

    size_t stacksize = proc->engine->opts.stacksize;
    t->stack = lfi_box_mapat(proc->box, proc->box_info.max - stacksize,
        stacksize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (t->stack == (lfiptr) -1)
        goto err3;

    // TODO: set up argv, envp and auxv on the stack

    return t;
err3:
    lfi_ctx_free(t->ctx);
err2:
    free(t);
err1:
    return NULL;
}

// Create a new thread cloned from the given thread.
struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t)
{
    struct LFILinuxThread *new_t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!new_t)
        goto err1;
    struct LFIContext *ctx = lfi_ctx_new(t->proc->box, new_t);
    if (!ctx)
        goto err2;
    new_t->ctx = ctx;
    new_t->proc = t->proc;
    new_t->tid = next_tid(t->proc);
    if (new_t->tid == -1)
        goto err2;
    // Copy all registers.
    *lfi_ctx_regs(new_t->ctx) = *lfi_ctx_regs(t->ctx);

    return new_t;

err2:
    free(new_t);
err1:
    return NULL;
}

EXPORT int
lfi_thread_run(struct LFILinuxThread *p)
{
    return lfi_ctx_run(p->ctx, p->proc->entry);
}

EXPORT void
lfi_thread_free(struct LFILinuxThread *t)
{
    // Unmap the stack.
    size_t stacksize = t->proc->engine->opts.stacksize;
    lfi_box_munmap(t->proc->box, t->stack, stacksize);
    // Free the context.
    lfi_ctx_free(t->ctx);
    // Free the thread.
    free(t);
}
