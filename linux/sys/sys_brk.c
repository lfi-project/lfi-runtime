#include "align.h"
#include "lock.h"
#include "sys/sys.h"

uintptr_t
sys_brk(struct LFILinuxThread *t, lfiptr addr)
{
    struct LFILinuxProc *p = t->proc;
    LOCK_WITH_DEFER(&p->lk_brk, lk_brk);

    lfiptr brkp = p->brkbase + p->brksize;
    if (addr != 0) {
        if (!ptrcheck(t, addr))
            return -1;
        brkp = addr;
    }
    size_t brkmaxsize = t->proc->engine->opts.brk_control ?
        t->proc->engine->opts.brk_size :
        BRKMAXSIZE;

    if (brkp < p->brkbase)
        brkp = p->brkbase;
    if (brkp >= p->brkbase + brkmaxsize)
        brkp = p->brkbase + brkmaxsize;

    size_t newsize = brkp - p->brkbase;
    assert(newsize < brkmaxsize);

    if (newsize == p->brksize)
        return brkp;

    const int MAP_FLAGS = LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS;
    const int MAP_PROT = LFI_PROT_READ | LFI_PROT_WRITE;

    if (brkp >= p->brkbase + p->brksize) {
        LOCK_WITH_DEFER(&p->lk_box, lk_box);
        lfiptr map;
        if (p->brksize == 0) {
            map = lfi_box_mapat(p->box, p->brkbase, newsize, MAP_PROT,
                MAP_FLAGS, -1, 0);
        } else {
            lfiptr next = ceilp(p->brkbase + p->brksize,
                lfi_opts(p->engine->engine).pagesize);
            map = lfi_box_mapat(p->box, next, newsize - p->brksize, MAP_PROT,
                MAP_FLAGS, -1, 0);
        }
        if (map == (lfiptr) -1)
            return -1;
    }
    p->brksize = newsize;
    LOG(p->engine, "sys_brk(%lx) = %lx", addr, p->brkbase + p->brksize);
    return p->brkbase + p->brksize;
}
