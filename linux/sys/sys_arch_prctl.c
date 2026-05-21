#include "sys/sys.h"

#define LINUX_ARCH_SET_FS 0x1002

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr)
{
#if defined(LFI_ARCH_X64)
    switch (code) {
    case LINUX_ARCH_SET_FS:
#if defined(SYS_MINIMAL) && !defined(CTXREG)
        // No TLS support in sys_minimal mode without CTXREG.
        return -LINUX_ENOSYS;
#else
        lfi_ctx_set_tp(t->ctx, addr);
        return 0;
#endif
    default:
        return -LINUX_EINVAL;
    }
#else
    return -LINUX_ENOSYS;
#endif
}
