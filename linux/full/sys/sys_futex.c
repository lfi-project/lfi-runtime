#include "full/futex.h"
#include "full/sys/sys.h"

#include <stdalign.h>

static long
futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    lfiptr timeoutp)
{
    struct TimeSpec timeout;
    if (timeoutp) {
        if (!bufcheck(t, timeoutp, sizeof(timeout), alignof(struct TimeSpec)))
            return -LINUX_EFAULT;
        lfi_box_copyfm(t->proc->box, &timeout, timeoutp, sizeof(timeout));
    }

    return host_futex_wait(t, uaddr, op, val, timeoutp ? &timeout : NULL);
}

long
full_sys_futex(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    uint64_t timeoutp, lfiptr uaddr2p, uint32_t val3)
{
    (void) val3;

    if (uaddrp & 3)
        return -LINUX_EINVAL;

    // The host futex operates on the sandbox memory directly. If the sandbox
    // has unmapped it, the host fails the operation with EFAULT.
    uint32_t *uaddr = bufhost(t, uaddrp, sizeof(uint32_t), alignof(uint32_t));
    if (!uaddr)
        return -LINUX_EFAULT;

    uint32_t val2 = (uint32_t) timeoutp;
    uint32_t *uaddr2;
    switch (op & ~LINUX_FUTEX_PRIVATE_FLAG) {
    case LINUX_FUTEX_WAIT:
        return futex_wait(t, uaddr, op, val, timeoutp);
    case LINUX_FUTEX_WAKE:
        return host_futex_wake(t, uaddr, op, val);
    case LINUX_FUTEX_REQUEUE:
        uaddr2 = bufhost(t, uaddr2p, sizeof(uint32_t), alignof(uint32_t));
        if (!uaddr2)
            return -LINUX_EFAULT;
        return host_futex_requeue(t, uaddr, op, val, val2, uaddr2);
    default:
        // Like Linux for an unknown operation. Also used for the operations
        // Linux has that are not supported here.
        LOG(t->proc->engine, "unsupported futex op %d", op);
        return -LINUX_ENOSYS;
    }
}
