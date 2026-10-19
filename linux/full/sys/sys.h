#pragma once

// Shared declarations for the full configuration's syscall handlers.

#include "full/full.h"
#include "sys/sys.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Dispatch table entries for full_syshandle. Each handled syscall stores its
// result and returns true to the caller.
#define FULL_SYS(SYSNAME, expr)   \
    case LINUX_SYS_##SYSNAME:     \
        *o_ret = (uintptr_t) (expr); \
        return true;

// Returns true if the buffer is valid and properly aligned.
static inline bool
bufcheck(struct LFILinuxThread *t, lfiptr p, size_t size, size_t align)
{
    if (!lfi_box_bufvalid(t->proc->box, p, size))
        return false;
    if (p % align != 0)
        return false;
    return true;
}

// Converts a sandbox buffer to a host buffer. Returns NULL if the buffer check
// fails. Accessing sandbox memory directly is open to TOCTOU, so only use this
// where that is not a concern. Assumes the sandbox is in the same address
// space as the host.
static inline void *
bufhost(struct LFILinuxThread *t, lfiptr p, size_t size, size_t align)
{
    if (!bufcheck(t, p, size, align))
        return NULL;
    return (void *) lfi_box_l2p(t->proc->box, p);
}

#define LINUX_PROT_SEM      8

#define LINUX_MAP_SHARED    1
#define LINUX_MAP_PRIVATE   2
#define LINUX_MAP_FIXED     16
#define LINUX_MAP_ANONYMOUS 32
#define LINUX_MAP_DENYWRITE 2048
#define LINUX_MAP_NORESERVE 16384

#define LINUX_FUTEX_WAIT         0
#define LINUX_FUTEX_WAKE         1
#define LINUX_FUTEX_REQUEUE      3
#define LINUX_FUTEX_PRIVATE_FLAG 128

#define LINUX_CLONE_VM             0x100
#define LINUX_CLONE_FS             0x200
#define LINUX_CLONE_FILES          0x400
#define LINUX_CLONE_SIGHAND        0x800
#define LINUX_CLONE_VFORK          0x4000
#define LINUX_CLONE_THREAD         0x10000
#define LINUX_CLONE_SYSVSEM        0x40000
#define LINUX_CLONE_SETTLS         0x80000
#define LINUX_CLONE_PARENT_SETTID  0x100000
#define LINUX_CLONE_CHILD_CLEARTID 0x200000
#define LINUX_CLONE_DETACHED       0x400000
#define LINUX_CLONE_CHILD_SETTID   0x1000000
#define LINUX_CLONE_IO             0x80000000UL

#define LINUX_SIGKILL 9
#define LINUX_SIGCHLD 17
#define LINUX_SIGSTOP 19
#define LINUX_NSIG    64

#define LINUX_SIG_BLOCK   0
#define LINUX_SIG_UNBLOCK 1
#define LINUX_SIG_SETMASK 2

uintptr_t
full_sys_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot,
    int flags, int fd, off_t off);

int
full_sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length,
    int prot);

int
full_sys_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t length);

uintptr_t
full_sys_clone(struct LFILinuxThread *t, uint64_t flags, lfiptr stack,
    lfiptr ptidp, lfiptr ctidp, uint64_t tls);

long
full_sys_futex(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    uint64_t timeoutp, lfiptr uaddr2p, uint32_t val3);

int
full_sys_set_tid_address(struct LFILinuxThread *t, lfiptr ctidp);

int
full_sys_gettid(struct LFILinuxThread *t);

int
full_sys_rt_sigprocmask(struct LFILinuxThread *t, int how, lfiptr setp,
    lfiptr oldsetp, size_t sigsetsize);

// The handlers below are only reachable in passthrough mode
// (LFILinuxOptions.sys_passthrough).

// Forwards the syscall to the host unmodified (UNSAFE).
uintptr_t
full_sys_passthrough(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5);

int
full_sys_rt_sigaction(struct LFILinuxThread *t, int sig, lfiptr actp,
    lfiptr oldactp, size_t sigsetsize);
