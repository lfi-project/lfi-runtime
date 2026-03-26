#pragma once

#include "arch_sys.h"
#include "proc.h"

#define LFI_SYS_pause 1024

_Static_assert(LFI_SYS_pause > LINUX_SYS_LAST, "LFI_SYS_pause invalid");

#define LFI_SYS_jitcode_mmap     1025
#define LFI_SYS_jitcode_munmap   1026
#define LFI_SYS_jitcode_create   1027
#define LFI_SYS_jitcode_create2  1028
#define LFI_SYS_jitcode_modify   1029
#define LFI_SYS_jitcode_delete   1030
#define LFI_SYS_jitcode_commit   1031
#define LFI_SYS_jitcode_decommit 1032

#define SYS(SYSNAME, expr)    \
    case LINUX_SYS_##SYSNAME: \
        handled = true;       \
        r = expr;             \
        break;

#define LFI(SYSNAME, expr)  \
    case LFI_SYS_##SYSNAME: \
        handled = true;     \
        r = expr;           \
        break;

// Generic syscall handler.
uintptr_t
syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5);
