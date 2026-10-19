#include "proc.h"

#include "elfload.h"
#include "elfsym.h"
#include "hooks.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

EXPORT struct LFILinuxProc *
lfi_proc_new(struct LFILinuxEngine *engine)
{
    struct LFIBox *box = lfi_box_new(engine->engine);
    if (!box)
        return NULL;

    struct LFILinuxProc *proc = calloc(sizeof(struct LFILinuxProc), 1);
    if (!proc) {
        lfi_box_free(box);
        return NULL;
    }
    proc->engine = engine;
    proc->box = box;
    proc->box_info = lfi_box_info(box);

    pthread_mutex_init(&proc->lk_proc, NULL);
    pthread_mutex_init(&proc->lk_brk, NULL);

    lfi_box_setdata(box, proc);

    if (!full_proc_init(proc)) {
        pthread_mutex_destroy(&proc->lk_brk);
        pthread_mutex_destroy(&proc->lk_proc);
        lfi_box_free(box);
        free(proc);
        return NULL;
    }

    return proc;
}

EXPORT struct LFIBox *
lfi_proc_box(struct LFILinuxProc *proc)
{
    return proc->box;
}

static bool
proc_load(struct LFILinuxProc *proc, int prog_fd, const uint8_t *prog,
    size_t prog_size, const char *prog_path, int map_op)
{
    bool debug = proc->engine->opts.debug;
    // Make the program path absolute if necessary.
    if (debug && prog_path && prog_path[0] != '/') {
        char cwd[FILENAME_MAX];
        if (getcwd(cwd, sizeof(cwd)) == cwd) {
            size_t len = strlen(cwd) + 1 + strlen(prog_path) + 1;
            proc->prog_path = malloc(len);
            if (proc->prog_path)
                snprintf(proc->prog_path, len, "%s/%s", cwd, prog_path);
        }
    } else if (debug && prog_path) {
        proc->prog_path = strndup(prog_path, FILENAME_MAX);
    }

    struct ELFLoadInfo info;
    if (!elf_load(proc, proc->prog_path, prog_fd, prog, prog_size, map_op,
            &info))
        return false;

    proc->entry = info.elfentry;
    proc->elfinfo = info;

    bool ok = elf_loadsyms(proc, prog, prog_size);
    if (!ok)
        LOG(proc->engine,
            "could not find .dynsym/.dynstr: dynamic symbol lookup will be unavailable");

    proc->brkbase = info.lastva;
    proc->brksize = 0;

    size_t brkmaxsize = proc->engine->opts.brk_control ?
        proc->engine->opts.brk_size :
        BRKMAXSIZE;

    // Reserve the brk region. Pages above the break stay part of the
    // reservation, which counts as unmapped for mprotect.
    if (brkmaxsize > 0) {
        lfiptr brkregion = lfi_box_mapat(proc->box, proc->brkbase, brkmaxsize,
            LFI_PROT_NONE,
            LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS | LFI_MAP_RESERVE, -1, 0);
        if (brkregion == (lfiptr) -1)
            return false;
    }

    return true;
}

EXPORT bool
lfi_proc_load(struct LFILinuxProc *proc, const uint8_t *prog, size_t prog_size,
    const char *prog_path)
{
    return proc_load(proc, -1, prog, prog_size, prog_path, ELF_MAP);
}

EXPORT bool
lfi_proc_load_remap(struct LFILinuxProc *proc, const uint8_t *prog, size_t prog_size, const char *prog_path)
{
    return proc_load(proc, -1, prog, prog_size, prog_path, ELF_REMAP);
}

EXPORT bool
lfi_proc_load_fd(struct LFILinuxProc *proc, int fd, const char *prog_path)
{
    off_t cur = lseek(fd, 0, SEEK_CUR);
    off_t prog_size = lseek(fd, 0, SEEK_END);
    if (prog_size < 0)
        return false;
    lseek(fd, cur, SEEK_SET);

    lseek(fd, 0, SEEK_SET);
    void *prog_data = mmap(NULL, prog_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (prog_data == (void *) -1)
        return false;

    bool ok = proc_load(proc, fd, prog_data, prog_size, prog_path, ELF_MAP);
    munmap(prog_data, prog_size);
    return ok;
}

EXPORT bool
lfi_proc_load_file(struct LFILinuxProc *proc, const char *prog_path)
{
    FILE *f = fopen(prog_path, "r");
    if (!f) {
        LOG(proc->engine, "%s: file does not exist", prog_path);
        return false;
    }
    bool ok = lfi_proc_load_fd(proc, fileno(f), prog_path);
    fclose(f);
    return ok;
}

void
proc_destroy(struct LFILinuxProc *proc)
{
    full_proc_free(proc);
    free(proc->dynsym.data);
    free(proc->dynstr.data);
    lfi_box_free(proc->box);
    free(proc->prog_path);
    pthread_mutex_destroy(&proc->lk_brk);
    pthread_mutex_destroy(&proc->lk_proc);
    free(proc);
}

EXPORT bool
lfi_proc_exited(struct LFILinuxProc *proc, int *code)
{
    if (!atomic_load_explicit(&proc->terminating, memory_order_acquire))
        return false;
    if (code)
        *code = atomic_load_explicit(&proc->exit_code, memory_order_relaxed);
    return true;
}

EXPORT void
lfi_proc_free(struct LFILinuxProc *proc)
{
    // Wait for any threads the runtime spawned for the sandbox to leave it.
    full_proc_join_threads(proc);

    // If any host threads are still lazily attached, defer the actual free
    // until the last detach happens. The detach path observes pending_free
    // under lk_proc and calls proc_destroy itself.
    lock(&proc->lk_proc);
    if (atomic_load_explicit(&proc->attached_threads,
            memory_order_relaxed) > 0) {
        proc->pending_free = true;
        unlock(&proc->lk_proc);
        return;
    }
    unlock(&proc->lk_proc);

    proc_destroy(proc);
}
