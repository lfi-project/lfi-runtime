#include "full/sys/sys.h"

// Size of the kernel's struct sigaction (handler, flags, restorer, mask) for
// a sigsetsize-byte mask.
#define KSIGACTION_SIZE(sigsetsize) (3 * sizeof(uint64_t) + (sigsetsize))

// Stub: no signal is ever delivered to the sandbox, so handlers are accepted
// and discarded. The old action reads back as SIG_DFL with no flags and an
// empty mask, which is what Linux reports for a signal that was never set.
int
full_sys_rt_sigaction(struct LFILinuxThread *t, int sig, lfiptr actp,
    lfiptr oldactp, size_t sigsetsize)
{
    if (sigsetsize != sizeof(uint64_t))
        return -LINUX_EINVAL;
    if (sig < 1 || sig > LINUX_NSIG)
        return -LINUX_EINVAL;
    if (actp && (sig == LINUX_SIGKILL || sig == LINUX_SIGSTOP))
        return -LINUX_EINVAL;
    if (actp &&
        !lfi_box_bufvalid(t->proc->box, actp, KSIGACTION_SIZE(sigsetsize)))
        return -LINUX_EFAULT;
    if (oldactp) {
        uint8_t old[KSIGACTION_SIZE(sizeof(uint64_t))] = { 0 };
        if (!lfi_box_copyto_checked(t->proc->box, oldactp, old, sizeof(old)))
            return -LINUX_EFAULT;
    }
    LOG(t->proc->engine, "rt_sigaction(%d): ignored", sig);
    return 0;
}
