#include "sys/sys.h"
#include "align.h"

uintptr_t
sys_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot, int flags, int fd, off_t off)
{
    if (length == 0)
        return -LINUX_EINVAL;
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    length = ceilp(length, pagesize);

    // Any mmap flag outside this list is rejected.
    const int illegal_mask = ~LINUX_MAP_ANONYMOUS &
        ~LINUX_MAP_PRIVATE &
        ~LINUX_MAP_NORESERVE &
        ~LINUX_MAP_DENYWRITE &
        ~LINUX_MAP_FIXED;
    if ((flags & illegal_mask) != 0) {
        LOG(t->proc->engine, "invalid mmap flag: not one of MAP_ANONYMOUS, MAP_PRIVATE, MAP_FIXED");
        return -LINUX_EINVAL;
    }

    lfiptr i_addrp = addrp;

    int r;
    if ((flags & LFI_MAP_FIXED) != 0) {
        addrp = truncp(addrp, pagesize);
        if (!ptrcheck(t, addrp))
            return -1;
        r = proc_mapat(t->proc, addrp, length, prot, flags, fd, off);
    } else {
        r = proc_mapany(t->proc, length, prot, flags, fd, off, &addrp);
    }
    if (r < 0) {
        LOG(t->proc->engine, "sys_mmap((%lx), %ld, %d, %d, %d, %ld) = %d", i_addrp, length, prot, flags, fd, (long) off, r);
        return r;
    }
    lfiptr ret = addrp;
    LOG(t->proc->engine, "sys_mmap(%lx (%lx), %ld, %d, %d, %d, %ld) = %lx", addrp, i_addrp, length, prot, flags, fd, (long) off, ret);
    return ret;
}
