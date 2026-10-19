#include "sys.h"

#include "hooks.h"
#include "sys/sys.h"

#include <stdlib.h>

uintptr_t
syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
    // Leave now if another thread asked the sandbox to terminate. This is the
    // only point where an arbitrary thread can be told to go.
    sys_checkpoint(t);

    uintptr_t r = -LINUX_ENOSYS;

    // The full configuration (if built in) gets the first look at every
    // syscall and may override anything below.
    if (full_syshandle(t, sysno, a0, a1, a2, a3, a4, a5, &r))
        return r;

    // clang-format off
    switch (sysno) {
#ifdef LINUX_SYS_arch_prctl
    SYS(arch_prctl,
            sys_arch_prctl(t, a0, a1))
#endif
    SYS(brk,
            sys_brk(t, a0))
    SYS(exit_group,
            sys_exit_group(t, a0))
    SYS(exit,
            sys_exit(t, a0))

    // LFI syscall for pausing execution.
    LFI(pause,
            sys_lfi_pause(t))

    // Unsupported syscalls that we ignore or purposefully return ENOSYS for.
    SYS(set_tid_address,
            sys_ignore(t, "set_tid_address"))
    SYS(mremap,
            sys_nosys(t, "mremap"))
    SYS(mprotect,
            sys_nosys(t, "mprotect"))
    // clang-format on
    default:
        LOG(t->proc->engine, "unknown syscall: %ld", sysno);

        if (t->proc->engine->opts.exit_unknown_syscalls) {
            ERROR("terminating due to unknown syscall: %ld", sysno);
            exit(1);
        }
    }

    return r;
}
