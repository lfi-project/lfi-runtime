#include "proc.h"

#include "buf.h"
#include "config.h"
#include "cwalk.h"
#include "elfload.h"
#include "fd.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"

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

    pthread_mutex_init(&proc->lk_box, NULL);
    pthread_mutex_init(&proc->lk_brk, NULL);

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
#else
        LOG(proc->engine,
            "warning: found dynamic interpreter, but CONFIG_ENABLE_DYLD is false");
#endif
        free(interp_path);
    }

    struct ELFLoadInfo info;
    if (!elf_load(proc, prog, prog_size, interp.data, interp.size, true, &info))
        return false;

    proc->brkbase = info.lastva;
    proc->brksize = 0;

    // Reserve the brk region.
    lfiptr brkregion = lfi_box_mapat(proc->box, proc->brkbase, BRKMAXSIZE,
        LFI_PROT_NONE, LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (brkregion == (lfiptr) -1)
        return false;

    lfiptr entry = info.elfentry;
    if (interp.data != NULL)
        entry = info.ldentry;
    proc->entry = entry;
    proc->elfinfo = info;

    return true;
}

EXPORT void
lfi_proc_free(struct LFILinuxProc *proc)
{
    fdclear(&proc->fdtable);
    free(proc);
}

int
proc_mapany(struct LFILinuxProc *p, size_t size, int prot, int flags, int fd,
        off_t offset, lfiptr *o_mapstart)
{
    int kfd = -1;
    if (fd >= 0) {
        // TODO: fdrelease this FDFile when this region is fully unmapped.
        struct FDFile* f = fdget(&p->fdtable, fd);
        if (!f)
            return -LINUX_EBADF;
        if (f->filefd) {
            kfd = f->filefd(f->dev);
        } else {
            return -LINUX_EACCES;
        }
    }
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    lfiptr addr = lfi_box_mapany(p->box, size, prot, flags, kfd, offset);
    if (addr == (lfiptr) -1)
        return -LINUX_EINVAL;
    *o_mapstart = (uintptr_t) addr;
    return 0;
}

int
proc_mapat(struct LFILinuxProc* p, lfiptr start, size_t size, int prot, int flags,
        int fd, off_t offset)
{
    int kfd = -1;
    if (fd >= 0) {
        // TODO: fdrelease this FDFile when this region is fully unmapped.
        struct FDFile *f = fdget(&p->fdtable, fd);
        if (!f)
            return LINUX_EBADF;
        if (f->filefd) {
            kfd = f->filefd(f->dev);
        } else {
            return -LINUX_EACCES;
        }
    }
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    lfiptr addr = lfi_box_mapat(p->box, start, size, prot, flags, kfd, offset);
    if (addr == (lfiptr) -1)
        return -LINUX_EINVAL;
    return 0;
}

int
proc_unmap(struct LFILinuxProc* p, lfiptr start, size_t size)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    return lfi_box_munmap(p->box, start, size);
}
