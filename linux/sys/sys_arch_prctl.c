#include "sys/sys.h"

#include <sys/syscall.h>
#include <unistd.h>

#define LINUX_ARCH_SET_FS       0x1002
#define LINUX_ARCH_SHSTK_ENABLE 0x5001
#define LINUX_ARCH_SHSTK_STATUS 0x5005
#define LINUX_ARCH_SHSTK_SHSTK  0x1

static int
host_shstk_enabled(void)
{
#if defined(__linux__) && defined(SYS_arch_prctl)
    unsigned long features = 0;
    int r = syscall(SYS_arch_prctl, LINUX_ARCH_SHSTK_STATUS, &features);
    if (r < 0)
        return host_err(errno);
    if ((features & LINUX_ARCH_SHSTK_SHSTK) == 0)
        return -LINUX_EINVAL;
    return 0;
#else
    return -LINUX_ENOSYS;
#endif
}

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr)
{
#if defined(LFI_ARCH_X64)
    switch (code) {
    case LINUX_ARCH_SET_FS:
#ifdef SYS_MINIMAL
        // No TLS support in sys_minimal mode.
        return -LINUX_ENOSYS;
#else
        lfi_ctx_set_tp(t->ctx, addr);
        return 0;
#endif
    case LINUX_ARCH_SHSTK_ENABLE:
        if (addr == LINUX_ARCH_SHSTK_SHSTK)
            return host_shstk_enabled();
        return -LINUX_EINVAL;
    default:
        return -LINUX_EINVAL;
    }
#else
    return -LINUX_ENOSYS;
#endif
}
