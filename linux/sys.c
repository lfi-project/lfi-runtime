#include "sys/sys.h"

#include "arch_sys.h"
#include "config.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>
#include <stdlib.h>

#define SYS(SYSNO, expr)    \
    case LINUX_SYS_##SYSNO: \
        r = expr;           \
        break;

uintptr_t
syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
    uintptr_t r = -LINUX_ENOSYS;
    switch (sysno) {
        // clang-format off
    SYS(getpid,
            0)
#ifdef LINUX_SYS_arch_prctl
    SYS(arch_prctl,
            sys_arch_prctl(t, a0, a1))
#endif
    SYS(set_tid_address,
            sys_set_tid_address(t, a0))
    SYS(ioctl,
            sys_ioctl(t, a0, a1, a2, a3, a4, a5))
    SYS(write,
            sys_write(t, a0, a1, a2))
    SYS(writev,
            sys_writev(t, a0, a1, a2))
    SYS(exit_group,
            sys_exit_group(t, a0))
    SYS(brk,
            sys_brk(t, a0))
    SYS(mmap,
            sys_mmap(t, a0, a1, a2, a3, a4, a5))
    SYS(mprotect,
            sys_mprotect(t, a0, a1, a2))
    SYS(munmap,
            sys_munmap(t, a0, a1))
    // clang-format on
    default:
        LOG(t->proc->engine, "unknown syscall: %ld", sysno);

        if (t->proc->engine->opts.exit_unknown_syscalls) {
            LOG_("terminating due to unknown syscall: %ld", sysno);
            exit(1);
        }
    }

    return r;
}
