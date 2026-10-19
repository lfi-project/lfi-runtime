#pragma once

// Shared declarations for the full configuration's syscall handlers.

#include "full/full.h"
#include "sys/sys.h"

#include <stdint.h>

// Dispatch table entries for full_syshandle. Each handled syscall stores its
// result and returns true to the caller.
#define FULL_SYS(SYSNAME, expr)   \
    case LINUX_SYS_##SYSNAME:     \
        *o_ret = (uintptr_t) (expr); \
        return true;

#define LINUX_PROT_SEM      8

#define LINUX_MAP_SHARED    1
#define LINUX_MAP_PRIVATE   2
#define LINUX_MAP_FIXED     16
#define LINUX_MAP_ANONYMOUS 32
#define LINUX_MAP_DENYWRITE 2048
#define LINUX_MAP_NORESERVE 16384

#define LINUX_SIGKILL 9
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

// The handlers below are only reachable in passthrough mode
// (LFILinuxOptions.sys_passthrough).

// Forwards the syscall to the host unmodified (UNSAFE).
uintptr_t
full_sys_passthrough(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5);

int
full_sys_rt_sigaction(struct LFILinuxThread *t, int sig, lfiptr actp,
    lfiptr oldactp, size_t sigsetsize);

int
full_sys_rt_sigprocmask(struct LFILinuxThread *t, int how, lfiptr setp,
    lfiptr oldsetp, size_t sigsetsize);
