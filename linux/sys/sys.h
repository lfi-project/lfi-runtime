#pragma once

#include "defer.h"
#include "fd.h"
#include "host.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"
#include "path.h"
#include "proc.h"

#include <assert.h>
#include <fcntl.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define SIGLFI SIGUSR1

// Returns true if the pointer is valid for the sandbox.
static inline bool
ptrcheck(struct LFILinuxThread *t, lfiptr p)
{
    return lfi_box_ptrvalid(t->proc->box, p);
}

// Returns true if the buffer is valid and properly aligned.
static inline bool
bufcheck(struct LFILinuxThread *t, lfiptr p, size_t size, size_t align)
{
    if (!ptrcheck(t, p))
        return false;
    if (!ptrcheck(t, p + size - 1))
        return false;
    if (p % align != 0)
        return false;
    return true;
}

// Converts a sandbox buffer to a host buffer. Returns NULL if the buffer check
// fails. Be careful when using this function: directly accessing a sandbox
// buffer can open us to TOCTOU attacks. In certain cases, TOCTOU is not
// possible or is not a concern, so we can use this function to more
// efficiently directly access the sandbox. For a safer version, use copyout
// and copyin.
// Use of this function assumes the sandbox is in the same address space as the
// host.
static inline void *
bufhost(struct LFILinuxThread *t, lfiptr p, size_t size, size_t align)
{
    if (!bufcheck(t, p, size, align))
        return NULL;
    return (void *) lfi_box_l2p(t->proc->box, p);
}

static inline void *
ptrhost(struct LFILinuxThread *t, lfiptr p)
{
    if (!ptrcheck(t, p))
        return NULL;
    return (void *) lfi_box_l2p(t->proc->box, p);
}

// Allocates a buffer and copies the sandbox data to it. Assumes the sandbox
// buffer has already been checked with bufcheck.
static inline void *
copyout(struct LFILinuxThread *t, lfiptr p, size_t size)
{
    assert(bufcheck(t, p, size, 1));
    void *host = malloc(size);
    if (!host)
        return NULL;
    return lfi_box_copyfm(t->proc->box, host, p, size);
}

static inline void
copyin(struct LFILinuxThread *t, lfiptr p, void *hostp, size_t size)
{
    assert(bufcheck(t, p, size, 1));
    lfi_box_copyto(t->proc->box, p, hostp, size);
}

// Checks and copies the given sandbox path into host memory. The user must
// free the returned path buffer when finished.
static inline char *
pathcopy(struct LFILinuxThread *t, lfiptr pathp)
{
    if (!ptrcheck(t, pathp))
        return NULL;
    char *host = malloc(FILENAME_MAX);
    if (!host)
        return NULL;
    strncpy(host, (char *) lfi_box_p2l(t->proc->box, pathp), FILENAME_MAX - 1);
    host[FILENAME_MAX - 1] = 0;
    return host;
}

// Checks pathp, copies it into host memory, and resolves it to a host path.
// Places the resolved host path in host_path, and returns the copied sandbox
// path. The user must free the sandbox path.
static inline char *
pathcopyresolve(struct LFILinuxThread *t, lfiptr pathp, char *host_path,
    size_t host_size)
{
    char *path = pathcopy(t, pathp);
    if (!path)
        return NULL;
    LOCK_WITH_DEFER(&t->proc->cwd.lk, lk_cwd);
    if (!path_resolve(t->proc, path, host_path, host_size)) {
        free(path);
        return NULL;
    }
    return path;
}

uintptr_t
sys_ignore(struct LFILinuxThread *t, const char *name);

uintptr_t
sys_nosys(struct LFILinuxThread *t, const char *name);

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr);

int
sys_set_tid_address(struct LFILinuxThread *t, uintptr_t ctid);

int
sys_ioctl(struct LFILinuxThread *t, int fd, unsigned long request,
    uintptr_t va0, uintptr_t va1, uintptr_t va2, uintptr_t va3);

ssize_t
sys_write(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size);

ssize_t
sys_writev(struct LFILinuxThread *t, int fd, lfiptr iovp, size_t iovcnt);

uintptr_t
sys_exit_group(struct LFILinuxThread *t, int code);

uintptr_t
sys_brk(struct LFILinuxThread *t, lfiptr addr);

uintptr_t
sys_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot,
    int flags, int fd, off_t off);

int
sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot);

int
sys_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t length);

int
sys_gettid(struct LFILinuxThread *t);

int
sys_sched_yield(struct LFILinuxThread *t);

int
sys_sched_getaffinity(struct LFILinuxThread *t, int32_t pid,
    uint64_t cpusetsize, lfiptr maskaddr);

long
sys_futex(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    uint64_t timeoutp, lfiptr uaddr2p, uint32_t val3);

uintptr_t
sys_exit(struct LFILinuxThread *t, int code);

int
sys_clone(struct LFILinuxThread *t, uint64_t flags, uint64_t stack,
    uint64_t ptid, uint64_t ctid, uint64_t tls, uint64_t func);

ssize_t
sys_read(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size);

ssize_t
sys_readv(struct LFILinuxThread *t, int fd, lfiptr iovp, size_t iovcnt);

off_t
sys_lseek(struct LFILinuxThread *t, int fd, off_t offset, int whence);

ssize_t
sys_pread64(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size,
    off_t offset);

ssize_t
sys_pwrite64(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size,
    off_t offset);

int
sys_openat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, int flags,
    int mode);

int
sys_close(struct LFILinuxThread *t, int fd);

ssize_t
sys_getdents64(struct LFILinuxThread *t, int fd, lfiptr dirp, size_t count);

int
sys_newfstatat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    lfiptr statbufp, int flags);

int
sys_fchmod(struct LFILinuxThread *t, int fd, linux_mode_t mode);

int
sys_truncate(struct LFILinuxThread *p, lfiptr pathp, off_t length);

int
sys_ftruncate(struct LFILinuxThread *t, int fd, off_t length);

int
sys_fchown(struct LFILinuxThread *t, int fd, linux_uid_t owner,
    linux_gid_t group);

int
sys_fsync(struct LFILinuxThread *p, int fd);

int
sys_mkdirat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    linux_mode_t mode);

int
sys_unlinkat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, int flags);

int
sys_renameat(struct LFILinuxThread *t, int olddir, lfiptr oldpathp, int newdir,
    lfiptr newpathp);

int
sys_faccessat(struct LFILinuxThread *t, int dirfd, uintptr_t pathp, int mode);

ssize_t
sys_readlinkat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, lfiptr bufp,
    size_t size);

int
sys_chdir(struct LFILinuxThread *p, lfiptr pathp);

int
sys_fchdir(struct LFILinuxThread *t, int fd);

ssize_t
sys_getcwd(struct LFILinuxThread *t, lfiptr bufp, size_t size);

int
sys_nanosleep(struct LFILinuxThread *t, lfiptr reqp, lfiptr remp);

int
sys_clock_gettime(struct LFILinuxThread *t, linux_clockid_t clockid, lfiptr tp);

ssize_t
sys_getrandom(struct LFILinuxThread *t, lfiptr bufp, size_t buflen,
    unsigned int flags);

int
sys_fcntl(struct LFILinuxThread *t, int fd, int cmd, uintptr_t va0,
    uintptr_t va1, uintptr_t va2, uintptr_t va3);

linux_time_t
sys_time(struct LFILinuxThread *t, lfiptr tlocp);

int
sys_chmod(struct LFILinuxThread *t, lfiptr pathp, linux_mode_t mode);

uintptr_t
sys_passthrough(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5);

uintptr_t
sys_lfi_pause(struct LFILinuxThread *t);

long
sys_lfi_register(struct LFILinuxThread *t, lfiptr box_funcs, size_t n);

int
sys_prctl(struct LFILinuxThread *t, int op, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5);

int
sys_sysinfo(struct LFILinuxThread *t, lfiptr infop);

int
sys_uname(struct LFILinuxThread *t, lfiptr bufp);

int
sys_renameat2(struct LFILinuxThread *t, int olddir, lfiptr oldpathp, int newdir,
    lfiptr newpathp, unsigned int flags);

int
sys_getrlimit(struct LFILinuxThread *t, int resource, lfiptr rlimp);

int
sys_dup(struct LFILinuxThread *t, int oldfd);

int
sys_dup3(struct LFILinuxThread *t, int oldfd, int newfd, int flags);
