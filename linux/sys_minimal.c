#include "sys.h"
#include "sys/sys.h"

#include <assert.h>
#include <stdlib.h>

uintptr_t
syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
    uintptr_t r = -LINUX_ENOSYS;

    bool handled = false;

    // clang-format off
    switch (sysno) {
#ifdef LINUX_SYS_arch_prctl
    SYS(arch_prctl,
            sys_arch_prctl(t, a0, a1))
#endif
    SYS(brk,
            sys_brk(t, a0))
    SYS(mmap,
            sys_mmap(t, a0, a1, a2, a3, a4, a5))
    SYS(munmap,
            sys_munmap(t, a0, a1))
    // Clone signature is different on aarch64 and x86-64.
#if defined(LFI_ARCH_X64)
    // syscall: clone(flags, stack, ptid, ctid, tls, func)
    SYS(clone,
            sys_clone(t, a0, a1, a2, a3, a4, a5))
#elif defined(LFI_ARCH_ARM64)
    // syscall: clone(flags, stack, ptid, tls, ctid, func)
    SYS(clone,
            sys_clone(t, a0, a1, a2, a4, a3, a5))
#else
#error "invalid arch"
#endif
    SYS(exit_group,
            sys_exit_group(t, a0))
    SYS(exit,
            sys_exit(t, a0))

    // LFI syscall for pausing execution.
    LFI(pause,
            sys_lfi_pause(t))

    SYS(write,
            sys_write(t, a0, a1, a2))
    SYS(writev,
            sys_writev(t, a0, a1, a2))

    // Other syscalls.
    SYS(ioctl,
            sys_ioctl(t, a0, a1, a2, a3, a4, a5))

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

    (void) handled;

    return r;
}
