#pragma once

#include "fd.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>
#include <stdalign.h>
#include <stdlib.h>

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
sys_exit(struct LFILinuxThread *t, uint64_t code);

int
sys_clone(struct LFILinuxThread *t, uint64_t flags, uint64_t stack,
    uint64_t ptid, uint64_t ctid, uint64_t tls, uint64_t func);
