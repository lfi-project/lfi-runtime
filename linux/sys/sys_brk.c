#include "align.h"
#include "lock.h"
#include "sys/sys.h"

uintptr_t
sys_brk(struct LFILinuxThread *t, lfiptr addr)
{
    struct LFILinuxProc *p = t->proc;
    LOCK_WITH_DEFER(&p->lk_brk, lk_brk);

    lfiptr cur = p->brkbase + p->brksize;
    if (addr == 0)
        return cur;

    size_t brkmaxsize = t->proc->engine->opts.brk_control ?
        t->proc->engine->opts.brk_size :
        BRKMAXSIZE;

    // Like Linux, a request that cannot be honored (outside the brk region,
    // overlapping another mapping, or one we fail to map) leaves the break
    // unchanged and returns it; brk never reports an error code.
    if (addr < p->brkbase || addr > p->brkbase + brkmaxsize)
        return cur;
    if (!ptrcheck(t, addr))
        return cur;

    size_t newsize = addr - p->brkbase;
    if (newsize == p->brksize)
        return cur;

    const int MAP_FLAGS = LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS;
    const int MAP_PROT = LFI_PROT_READ | LFI_PROT_WRITE;

    size_t pagesize = lfi_opts(p->engine->engine).pagesize;
    lfiptr cur_end = ceilp(cur, pagesize);
    lfiptr new_end = ceilp(addr, pagesize);
    if (new_end > cur_end) {
        // Grow only over the brk reservation or free pages, never over a
        // mapping the sandbox made inside the brk region with mmap.
        lfiptr map = lfi_box_mapat_noreplace(p->box, cur_end, new_end - cur_end,
            MAP_PROT, MAP_FLAGS, -1, 0);
        if (map == (lfiptr) -1)
            return cur;
    } else if (new_end < cur_end) {
        // Like Linux, release whole pages above the new break: they return to
        // the reservation, so they come back zeroed and count as unmapped.
        // Fail rather than release a runtime mapping (such as a thread stack)
        // that was placed in a hole the sandbox unmapped below the break.
        lfiptr map = lfi_box_mapat_unpinned(p->box, new_end, cur_end - new_end,
            LFI_PROT_NONE, MAP_FLAGS | LFI_MAP_RESERVE, -1, 0);
        if (map == (lfiptr) -1)
            return cur;
    }
    p->brksize = newsize;
    LOG(p->engine, "sys_brk(%lx) = %lx", addr, p->brkbase + p->brksize);
    return p->brkbase + p->brksize;
}
