#include "sys/sys.h"

#define LINUX_ARCH_SET_FS 0x1002

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr)
{
#if defined(LFI_ARCH_X64)
    switch (code) {
    case LINUX_ARCH_SET_FS:
        lfi_ctx_set_tp(t->ctx, addr);
        return 0;
    default:
        return -LINUX_EINVAL;
    }
#else
    return -LINUX_ENOSYS;
#endif
}
