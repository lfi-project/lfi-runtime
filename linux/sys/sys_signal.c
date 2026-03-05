#include "sys/sys.h"

#include <assert.h>
#include <signal.h>
#include <string.h>

int
sys_rt_sigaction(struct LFILinuxThread *t, int sig, lfiptr actp, lfiptr oldp,
    uint64_t sigsetsize)
{
    if (sig < 1 || sig >= LINUX_NSIG)
        return -LINUX_EINVAL;
    if (sigsetsize != 8) {
        LOG(t->proc->engine, "rt_sigaction (%d): sigsetsize != 8", sig);
        return -LINUX_EINVAL;
    }

    // Write old action if requested.
    if (oldp) {
        void *ob = bufhost(t, oldp, sizeof(struct SigAction),
            alignof(struct SigAction));
        if (!ob)
            return -LINUX_EFAULT;
        struct SigAction *old = ob;
        if (t->proc->signals[sig].valid) {
            *old = t->proc->signals[sig].entry;
        } else {
            *old = (struct SigAction){ .handler = LINUX_SIG_DFL };
        }
    }

    if (!actp)
        return 0;

    void *ab = bufhost(t, actp, sizeof(struct SigAction),
        alignof(struct SigAction));
    if (!ab)
        return -LINUX_EFAULT;
    struct SigAction *act = ab;

    if (act->handler == LINUX_SIG_DFL || act->handler == LINUX_SIG_IGN) {
        t->proc->signals[sig].valid = false;
        return 0;
    }

    if ((act->flags & SA_RESTORER) == 0) {
        LOG(t->proc->engine, "TODO: rt_sigaction must use SA_RESTORER");
        return -LINUX_EINVAL;
    }

    if (!lfi_box_ptrvalid(t->proc->box, act->handler)) {
        LOG(t->proc->engine, "rt_sigaction: handler is invalid!\n");
        return -LINUX_EINVAL;
    }
    if ((act->handler & 0x1f) != 0) {
        LOG(t->proc->engine, "rt_sigaction: handler is not bundle aligned!\n");
        return -LINUX_EINVAL;
    }

    if (!lfi_box_ptrvalid(t->proc->box, act->restorer)) {
        LOG(t->proc->engine, "rt_sigaction: restorer is invalid!\n");
        return -LINUX_EINVAL;
    }
    // NOTE: upstream musl restorer is intentionally not aligned.
    if ((act->restorer & 0x1f) != 0) {
        LOG(t->proc->engine,
            "rt_sigaction: restorer(%lx) is not bundle aligned!\n",
            act->restorer);
        return -LINUX_EINVAL;
    }

    t->proc->signals[sig].entry = *act;
    t->proc->signals[sig].valid = true;

    LOG(t->proc->engine, "rt_sigaction (%d): handler: %lx", sig, act->handler);
    LOG(t->proc->engine, "sa_flags: %lx", act->flags);

    return 0;
}

uintptr_t
sys_rt_sigreturn(struct LFILinuxThread *t)
{
    LOG(t->proc->engine, "rt_sigreturn");
    lfi_ctx_exit(t->ctx, 0);
    __builtin_unreachable();
}
