#include "full/sys/sys.h"

// Stub: no signal is ever delivered to the sandbox, so the mask has no
// effect and is not tracked. The old mask always reads back as empty.
int
full_sys_rt_sigprocmask(struct LFILinuxThread *t, int how, lfiptr setp,
    lfiptr oldsetp, size_t sigsetsize)
{
    if (sigsetsize != sizeof(uint64_t))
        return -LINUX_EINVAL;
    if (setp) {
        if (how != LINUX_SIG_BLOCK && how != LINUX_SIG_UNBLOCK &&
            how != LINUX_SIG_SETMASK)
            return -LINUX_EINVAL;
        if (!lfi_box_bufvalid(t->proc->box, setp, sigsetsize))
            return -LINUX_EFAULT;
    }
    if (oldsetp) {
        uint64_t old = 0;
        if (!lfi_box_copyto_checked(t->proc->box, oldsetp, &old, sizeof(old)))
            return -LINUX_EFAULT;
    }
    LOG(t->proc->engine, "rt_sigprocmask(%d): ignored", how);
    return 0;
}
