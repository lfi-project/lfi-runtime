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
    // clang-format off
    switch (sysno) {
    SYS(getpid,
            0)
#ifdef LINUX_SYS_arch_prctl
    SYS(arch_prctl,
            sys_arch_prctl(t, a0, a1))
#endif
    SYS(ioctl,
            sys_ioctl(t, a0, a1, a2, a3, a4, a5))
    SYS(write,
            sys_write(t, a0, a1, a2))
    SYS(writev,
            sys_writev(t, a0, a1, a2))
    SYS(exit_group,
            sys_exit_group(t, a0))
    SYS(exit,
            sys_exit(t, a0))
    SYS(brk,
            sys_brk(t, a0))
    SYS(mmap,
            sys_mmap(t, a0, a1, a2, a3, a4, a5))
    SYS(mprotect,
            sys_mprotect(t, a0, a1, a2))
    SYS(munmap,
            sys_munmap(t, a0, a1))

    // Thread support calls.
    SYS(gettid,
            sys_gettid(t))
    SYS(futex,
            sys_futex(t, a0, a1, a2, a3, a4, a5))
#if defined(LFI_ARCH_X64)
    // syscall: clone(flags, stack, ptid, ctid, tls, func)
    SYS(clone,
            sys_clone(t, a0, a1, a2, a3, a4, a5))
#elif defined(LFI_ARCH_ARM64)
    // syscall: clone(flags, stack, ptid, tls, ctid, func)
    // Clone signature is different on aarch64 and x86-64.
    SYS(clone,
            sys_clone(t, a0, a1, a2, a4, a3, a5))
#else
#error "invalid arch"
#endif
    SYS(set_tid_address,
            sys_set_tid_address(t, a0))
    SYS(sched_getaffinity,
            sys_sched_getaffinity(t, a0, a1, a2))
    SYS(sched_setaffinity,
            sys_ignore(t, "sched_setaffinity"))
    SYS(sched_yield,
            sys_sched_yield(t))

    // Signals (also needed, at least as stubs, for threads).
    SYS(rt_sigaction,
            sys_ignore(t, "rt_sigaction"))
    SYS(rt_sigprocmask,
            sys_ignore(t, "rt_sigprocmask"))
    SYS(rt_sigreturn,
            sys_ignore(t, "rt_sigreturn"))
    SYS(sigaltstack,
            sys_ignore(t, "sigaltstack"))

    // File operations.
    // ...

    // Unsupported syscalls that we ignore or purposefully return ENOSYS for.
    SYS(set_robust_list,
            sys_ignore(t, "set_robust_list"))
    SYS(membarrier,
            sys_ignore(t, "membarrier"))
    SYS(madvise,
            sys_ignore(t, "madvise"))
    SYS(getrusage,
            sys_ignore(t, "getrusage"))
    SYS(statx,
            sys_nosys(t, "statx"))
    SYS(rseq,
            sys_nosys(t, "rseq"))
    SYS(prlimit64,
            sys_nosys(t, "prlimit64"))
    SYS(statfs,
            sys_nosys(t, "statfs"))
    SYS(getxattr,
            sys_nosys(t, "getxattr"))
    SYS(lgetxattr,
            sys_nosys(t, "lgetxattr"))
    SYS(socket,
            sys_nosys(t, "socket"))
    SYS(mremap,
            sys_nosys(t, "mremap"))
    SYS(utimensat,
            sys_nosys(t, "utimensat"))
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
