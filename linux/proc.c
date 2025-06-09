#include "proc.h"

#include "buf.h"
#include "config.h"
#include "cwalk.h"
#include "elfload.h"
#include "fd.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>

// First TID, to avoid using low TID numbers.
#define BASE_TID 10000

// May return -1 if there are no more available TIDs.
static int
next_tid(struct LFILinuxProc *p)
{
    // This is a place where we could enforce a maximum number of threads.
    return BASE_TID +
        atomic_fetch_add_explicit(&p->threads, 1, memory_order_relaxed);
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
    // Copy all registers.
    *lfi_ctx_regs(new_t->ctx) = *lfi_ctx_regs(t->ctx);

    return new_t;

err2:
    free(new_t);
err1:
    return NULL;
}

struct LFILinuxProc *
lfi_proc_new(struct LFILinuxEngine *engine, struct LFIBox *box)
{
    struct LFILinuxProc *proc = calloc(sizeof(struct LFILinuxProc), 1);
    if (!proc)
        return NULL;
    proc->engine = engine;
    proc->box = box;
    proc->box_info = lfi_box_info(box);

    pthread_mutex_init(&proc->lk_box, NULL);
    pthread_mutex_init(&proc->lk_brk, NULL);

    fdinit(engine, &proc->fdtable);

    return proc;
}

struct LFIBox *
lfi_proc_box(struct LFILinuxProc *proc)
{
    return proc->box;
}

bool
lfi_proc_load(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size)
{
    struct Buf interp = (struct Buf) { 0 };

#ifdef CONFIG_ENABLE_DYLD
    char *interp_path = elf_interp(prog, prog_size);
    if (interp_path) {
        if (cwk_path_is_absolute(interp_path)) {
            interp = buf_read_file(proc->engine, interp_path);
            if (!interp.data) {
                LOG(proc->engine, "error opening dynamic linker %s: %s",
                    interp_path, strerror(errno));
                free(interp_path);
                return false;
            }
            LOG(proc->engine, "using dynamic linker: %s", interp_path);
        } else {
            LOG(proc->engine,
                "dynamic linker ignored because it is relative path: %s",
                interp_path);
        }
        free(interp_path);
    }
#endif

    return false;
}

void
lfi_proc_free(struct LFILinuxProc *proc)
{
    fdclear(&proc->fdtable);
    free(proc);
}
