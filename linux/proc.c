#include "proc.h"

#include "buf.h"
#include "config.h"
#include "cwalk.h"
#include "elfload.h"
#include "elfsym.h"
#include "fd.h"
#include "host.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"
#include "path.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>

EXPORT struct LFILinuxProc *
lfi_proc_new(struct LFILinuxEngine *engine, struct LFIBox *box)
{
    struct LFILinuxProc *proc = calloc(sizeof(struct LFILinuxProc), 1);
    if (!proc)
        return NULL;
    proc->engine = engine;
    proc->box = box;
    proc->box_info = lfi_box_info(box);

    pthread_mutex_init(&proc->lk_proc, NULL);
    pthread_mutex_init(&proc->lk_clone, NULL);
    pthread_mutex_init(&proc->lk_threads, NULL);
    pthread_mutex_init(&proc->lk_box, NULL);
    pthread_mutex_init(&proc->lk_brk, NULL);
    pthread_mutex_init(&proc->cwd.lk, NULL);
    pthread_cond_init(&proc->cond_threads, NULL);

    if (engine->opts.wd) {
        int r = proc_chdir(proc, engine->opts.wd);
        if (r != 0) {
            LOG(engine, "error setting working directory to %s",
                engine->opts.wd);
            free(proc);
            return NULL;
        }
    }

    lfi_box_setdata(box, proc);

    fdinit(engine, &proc->fdtable);

    return proc;
}

EXPORT struct LFIBox *
lfi_proc_box(struct LFILinuxProc *proc)
{
    return proc->box;
}

EXPORT bool
lfi_proc_load(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size)
{
    struct Buf interp = (struct Buf) { 0 };

    char *interp_path = elf_interp(prog, prog_size);
    if (interp_path) {
#ifdef CONFIG_ENABLE_DYLD
        if (cwk_path_is_absolute(interp_path)) {
            char interp_host[FILENAME_MAX];
            if (!path_resolve(proc, interp_path, interp_host,
                    sizeof(interp_host))) {
                LOG(proc->engine,
                    "error opening dynamic linker %s: host path resolution failed",
                    interp_path);
                free(interp_path);
                return false;
            }
            interp = buf_read_file(proc->engine, interp_host);
            if (!interp.data) {
                LOG(proc->engine, "error opening dynamic linker %s: %s",
                    interp_path, strerror(errno));
                free(interp_path);
                return false;
            }
            LOG(proc->engine, "using sandbox dynamic linker: %s", interp_path);
        } else {
            LOG(proc->engine,
                "dynamic linker ignored because it is relative path: %s",
                interp_path);
        }
#else
        LOG(proc->engine,
            "warning: found dynamic interpreter, but CONFIG_ENABLE_DYLD is false");
#endif
        free(interp_path);
    }

    struct ELFLoadInfo info;
    if (!elf_load(proc, prog, prog_size, interp.data, interp.size, true, &info))
        return false;

    lfiptr entry = info.elfentry;
    if (interp.data != NULL)
        entry = info.ldentry;
    proc->entry = entry;
    proc->elfinfo = info;

    bool ok = elf_loadsyms(proc, prog, prog_size);
    if (!ok)
        LOG(proc->engine,
            "could not find .dynsym/.dynstr: dynamic symbol lookup will be unavailable");

    proc->brkbase = info.lastva;
    proc->brksize = 0;

    // Reserve the brk region.
    lfiptr brkregion = lfi_box_mapat(proc->box, proc->brkbase, BRKMAXSIZE,
        LFI_PROT_NONE, LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (brkregion == (lfiptr) -1)
        return false;

    return true;
}

EXPORT void
lfi_proc_free(struct LFILinuxProc *proc)
{
    lock(&proc->lk_proc);
    lock(&proc->lk_threads);
    while (proc->active_threads != 0) {
        pthread_cond_wait(&proc->cond_threads, &proc->lk_threads);
    }
    assert(proc->active_threads == 0);
    unlock(&proc->lk_threads);
    free(proc->dynsym.data);
    free(proc->dynstr.data);
    unlock(&proc->lk_proc);
    free(proc);
}

int
proc_mapany(struct LFILinuxProc *p, size_t size, int prot, int flags, int fd,
    off_t offset, lfiptr *o_mapstart)
{
    int kfd = -1;
    if (fd >= 0) {
        kfd = fdget(&p->fdtable, fd);
        if (kfd == -1)
            return -LINUX_EBADF;
    }
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    lfiptr addr = lfi_box_mapany(p->box, size, prot, flags, kfd, offset);
    if (addr == (lfiptr) -1)
        return -LINUX_EINVAL;
    *o_mapstart = (uintptr_t) addr;
    return 0;
}

int
proc_mapat(struct LFILinuxProc *p, lfiptr start, size_t size, int prot,
    int flags, int fd, off_t offset)
{
    int kfd = -1;
    if (fd >= 0) {
        kfd = fdget(&p->fdtable, fd);
        if (kfd == -1)
            return LINUX_EBADF;
    }
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    lfiptr addr = lfi_box_mapat(p->box, start, size, prot, flags, kfd, offset);
    if (addr == (lfiptr) -1)
        return -LINUX_EINVAL;
    return 0;
}

int
proc_unmap(struct LFILinuxProc *p, lfiptr start, size_t size)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    return lfi_box_munmap(p->box, start, size);
}

// Resolves a sandbox path to a host path and makes sure the host path exists
// and is a directory.
static int
chdircheck(struct LFILinuxProc *p, const char *path)
{
    char resolved[FILENAME_MAX];
    if (!path_resolve(p, path, resolved, sizeof(resolved)))
        return -LINUX_ENOENT;
    return host_isdir(resolved);
}

// Change the proc's cwd to the given sandbox path.
int
proc_chdir(struct LFILinuxProc *p, const char *path)
{
    int r;
    if (cwk_path_is_absolute(path)) {
        LOCK_WITH_DEFER(&p->cwd.lk, lk_cwd);
        if ((r = chdircheck(p, path)) < 0)
            return r;
        size_t len = strnlen(path, sizeof(p->cwd.path) - 1);
        memcpy(&p->cwd.path[0], path, len);
        p->cwd.path[len] = 0;
    } else {
        if (p->cwd.path[0] == 0)
            return -LINUX_ENOENT;

        char joined[FILENAME_MAX];
        LOCK_WITH_DEFER(&p->cwd.lk, lk_cwd);
        cwk_path_join(p->cwd.path, path, joined, sizeof(joined));
        if ((r = chdircheck(p, joined)) < 0)
            return r;
        strncpy(p->cwd.path, joined, sizeof(joined));
        p->cwd.path[sizeof(p->cwd.path) - 1] = 0;
    }
    return 0;
}
