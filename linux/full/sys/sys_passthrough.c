#include "full/sys/sys.h"

#ifdef __linux__
#include <errno.h>
#include <unistd.h>
#endif

// Passes a syscall to the host unmodified (UNSAFE). Nothing is checked:
// pointer arguments are handed to the host as is, which relies on sandbox
// pointers being valid host pointers, and file descriptors are host file
// descriptors. Only for testing and benchmarking.
uintptr_t
full_sys_passthrough(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
#ifdef __linux__
    long r = syscall(sysno, a0, a1, a2, a3, a4, a5);
    // The host is Linux, so its errno values are the Linux ones.
    if (r == -1)
        r = -errno;
    LOG(t->proc->engine,
        "passthrough: syscall %ld (%lx, %lx, %lx, %lx, %lx, %lx) = %ld",
        (long) sysno, a0, a1, a2, a3, a4, a5, r);
    return r;
#else
    (void) sysno, (void) a0, (void) a1, (void) a2, (void) a3, (void) a4,
        (void) a5;
    LOG(t->proc->engine, "passthrough requires a Linux host");
    return -LINUX_ENOSYS;
#endif
}
