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
