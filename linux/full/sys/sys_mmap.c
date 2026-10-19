#define _GNU_SOURCE

#include "align.h"
#include "full/sys/sys.h"

#ifdef __linux__
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#endif

// Checks that a host file descriptor can back a mapping, returning the error
// Linux gives if not. Every failure must be caught here: a MAP_FIXED mapping
// replaces whatever was at the address before the host mmap runs, so a failed
// host mmap would leave a hole where Linux keeps the old mapping. Only regular
// files and block devices are accepted, since whether any other file can be
// mapped is up to its driver and is only known once the host mmap runs (unlike
// Linux, this refuses mappable character devices such as /dev/zero). The file
// must be open for reading, and also for writing if the mapping is shared and
// writable, and the mapping must end within the largest file offset.
static int
filecheck(int fd, bool shared, int prot, size_t length, off_t off)
{
#ifdef __linux__
    int fl = fcntl(fd, F_GETFL);
    // Like Linux, an O_PATH descriptor cannot be mapped at all.
    if (fl < 0 || (fl & O_PATH) != 0)
        return -LINUX_EBADF;
    struct stat st;
    if (fstat(fd, &st) < 0)
        return -LINUX_EBADF;
    if (off < 0 || length > (size_t) INT64_MAX ||
        (uint64_t) off > (uint64_t) INT64_MAX - length)
        return -LINUX_EOVERFLOW;
    int mode = fl & O_ACCMODE;
    if (mode == O_WRONLY)
        return -LINUX_EACCES;
    if (shared && (prot & LFI_PROT_WRITE) && mode != O_RDWR)
        return -LINUX_EACCES;
    if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode))
        return -LINUX_ENODEV;
    return 0;
#else
    (void) fd, (void) shared, (void) prot, (void) length, (void) off;
    return -LINUX_EBADF;
#endif
}

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

    bool anon = (flags & LINUX_MAP_ANONYMOUS) != 0;
    // In passthrough mode file descriptors are host file descriptors, so a
    // file can be mapped directly, shared or private. Executable mappings are
    // still refused below, so a shared file mapping cannot alias code.
    bool file_ok = !anon && t->proc->engine->opts.sys_passthrough;

    // Any mmap flag outside this list is rejected.
    const int legal = LINUX_MAP_ANONYMOUS | LINUX_MAP_PRIVATE |
        LINUX_MAP_NORESERVE | LINUX_MAP_DENYWRITE | LINUX_MAP_FIXED |
        (file_ok ? LINUX_MAP_SHARED : 0);
    if ((flags & ~legal) != 0) {
        LOG(t->proc->engine,
            "invalid mmap flag: not one of MAP_ANONYMOUS, MAP_PRIVATE, MAP_FIXED");
        return -LINUX_EINVAL;
    }
    // MAP_SHARED is refused above unless this is a file mapping in
    // passthrough mode. Like Linux, a mapping with no type is rejected.
    int type = flags & (LINUX_MAP_SHARED | LINUX_MAP_PRIVATE);
    if (type != LINUX_MAP_PRIVATE && type != LINUX_MAP_SHARED)
        return -LINUX_EINVAL;

    // Otherwise, file-backed mappings need a file descriptor table, which the
    // full configuration does not have yet, so no descriptor is valid. Like
    // Linux, the descriptor is ignored for anonymous mappings.
    if (!anon && !file_ok) {
        LOG(t->proc->engine, "file-backed mmap is not supported");
        return -LINUX_EBADF;
    }

    // Like Linux, mmap ignores unknown protection bits (unlike mprotect).
    prot &= LFI_PROT_READ | LFI_PROT_WRITE | LFI_PROT_EXEC;
    if (prot & LFI_PROT_EXEC)
        return -LINUX_EACCES;

    int map_flags;
    if (anon) {
        map_flags = LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS;
        fd = -1;
        off = 0;
    } else {
        map_flags = type == LINUX_MAP_SHARED ? LFI_MAP_SHARED : LFI_MAP_PRIVATE;
    }

    bool fixed = (flags & LINUX_MAP_FIXED) != 0;
    if (fixed) {
        if (addrp % pagesize != 0)
            return -LINUX_EINVAL;
        if (!lfi_box_bufvalid(t->proc->box, addrp, length))
            return -LINUX_ENOMEM;
    }
    // Like Linux, the file is checked after the placement.
    if (!anon) {
        int r = filecheck(fd, type == LINUX_MAP_SHARED, prot, length, off);
        if (r < 0)
            return r;
    }

    lfiptr ret;
    if (fixed) {
        // Runtime-owned (pinned) mappings cannot be replaced.
        ret = lfi_box_mapat_unpinned(t->proc->box, addrp, length, prot,
            map_flags, fd, off);
    } else {
        ret = lfi_box_mapany(t->proc->box, length, prot, map_flags, fd, off);
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
