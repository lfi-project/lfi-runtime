#include "sys/sys.h"

#define LINUX_ARCH_SET_FS 0x1002
#define LINUX_ARCH_GET_FS 0x1003

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr)
{
#if defined(LFI_ARCH_X64)
    switch (code) {
    case LINUX_ARCH_SET_FS:
        lfi_ctx_set_tp(t->ctx, addr);
        return 0;
    case LINUX_ARCH_GET_FS: {
        uint64_t tp = lfi_ctx_get_tp(t->ctx);
        if (!lfi_box_copyto_checked(t->proc->box, addr, &tp, sizeof(tp)))
            return -LINUX_EFAULT;
        return 0;
    }
    default:
        return -LINUX_EINVAL;
    }
#else
    return -LINUX_ENOSYS;
#endif
}
