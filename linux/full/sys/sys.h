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

uintptr_t
full_sys_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot,
    int flags, int fd, off_t off);

int
full_sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length,
    int prot);

int
full_sys_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t length);
