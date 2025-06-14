#include "sys/sys.h"

#include <unistd.h>

// Pass system calls through unmodified (UNSAFE). This should only be used for
// testing/benchmarking.
uintptr_t
sys_passthrough(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
#ifdef __linux__
    LOG(t->proc->engine,
        "passthrough: syscall %ld (%lx, %lx, %lx, %lx, %lx, %lx)", sysno, a0,
        a1, a2, a3, a4, a5);
    return HOST_ERR(uintptr_t, syscall(sysno, a0, a1, a2, a3, a4, a5));
#else
    LOG(t->proc->engine, "passthrough requires linux");
    return -LINUX_ENOSYS;
#endif
}
