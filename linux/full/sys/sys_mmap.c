#include "align.h"
#include "full/sys/sys.h"

uintptr_t
full_sys_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot,
    int flags, int fd, off_t off)
{
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    if (length == 0)
        return -LINUX_EINVAL;
    // Like Linux, a length that overflows when rounded up to a page is out of
    // memory rather than invalid.
    if (length > SIZE_MAX - (pagesize - 1))
        return -LINUX_ENOMEM;
    if (off % pagesize != 0)
        return -LINUX_EINVAL;
    length = ceilp(length, pagesize);

    // Any mmap flag outside this list is rejected.
    const int illegal_mask = ~LINUX_MAP_ANONYMOUS & ~LINUX_MAP_PRIVATE &
        ~LINUX_MAP_NORESERVE & ~LINUX_MAP_DENYWRITE & ~LINUX_MAP_FIXED;
    if ((flags & illegal_mask) != 0) {
        LOG(t->proc->engine,
            "invalid mmap flag: not one of MAP_ANONYMOUS, MAP_PRIVATE, MAP_FIXED");
        return -LINUX_EINVAL;
    }
    // MAP_SHARED is refused above, so the mapping type must be MAP_PRIVATE;
    // Linux rejects a mapping with no type.
    if ((flags & LINUX_MAP_PRIVATE) == 0)
        return -LINUX_EINVAL;

    // File-backed mappings need a file descriptor table, which the full
    // configuration does not have yet, so no descriptor is valid. Like Linux,
    // the descriptor is ignored for anonymous mappings.
    if ((flags & LINUX_MAP_ANONYMOUS) == 0) {
        LOG(t->proc->engine, "file-backed mmap is not supported");
        return -LINUX_EBADF;
    }

    // Like Linux, mmap ignores unknown protection bits (unlike mprotect).
    prot &= LFI_PROT_READ | LFI_PROT_WRITE | LFI_PROT_EXEC;
    if (prot & LFI_PROT_EXEC)
        return -LINUX_EACCES;

    lfiptr ret;
    if ((flags & LINUX_MAP_FIXED) != 0) {
        if (addrp % pagesize != 0)
            return -LINUX_EINVAL;
        if (!lfi_box_bufvalid(t->proc->box, addrp, length))
            return -LINUX_ENOMEM;
        // Runtime-owned (pinned) mappings cannot be replaced.
        ret = lfi_box_mapat_unpinned(t->proc->box, addrp, length, prot,
            LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    } else {
        ret = lfi_box_mapany(t->proc->box, length, prot,
            LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    }
    if (ret == (lfiptr) -1) {
        LOG(t->proc->engine, "sys_mmap(%lx, %ld, %d, %d, %d, %ld) failed",
            addrp, length, prot, flags, fd, (long) off);
        return -LINUX_ENOMEM;
    }
    LOG(t->proc->engine, "sys_mmap(%lx, %ld, %d, %d, %d, %ld) = %lx",
        addrp, length, prot, flags, fd, (long) off, ret);
    return ret;
}
