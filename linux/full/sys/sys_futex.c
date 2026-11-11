#include "full/futex.h"
#include "full/sys/sys.h"

#include <stdalign.h>

// Converts the address of a futex word to a host pointer. Returns 0, or a
// negated Linux errno. The host futex operates on the sandbox memory directly.
// If the sandbox has unmapped it, the host handles it as Linux does.
static long
futex_addr(struct LFILinuxThread *t, lfiptr p, uint32_t **o_addr)
{
    if (p & 3)
        return -LINUX_EINVAL;
    *o_addr = bufhost(t, p, sizeof(uint32_t), alignof(uint32_t));
    if (!*o_addr)
        return -LINUX_EFAULT;
    return 0;
}

// Copies a timespec from the sandbox and checks it like Linux. Returns 0, or a
// negated Linux errno.
static long
read_timeout(struct LFILinuxThread *t, lfiptr timeoutp,
    struct TimeSpec *o_timeout)
{
    if (!bufcheck(t, timeoutp, sizeof(*o_timeout), 1))
        return -LINUX_EFAULT;
    lfi_box_copyfm(t->proc->box, o_timeout, timeoutp, sizeof(*o_timeout));
    if ((int64_t) o_timeout->sec < 0 || o_timeout->nsec >= 1000000000)
        return -LINUX_EINVAL;
    return 0;
}

static long
futex_wait(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    lfiptr timeoutp)
{
    long r;
    struct TimeSpec timeout;
    if (timeoutp && (r = read_timeout(t, timeoutp, &timeout)) < 0)
        return r;
    uint32_t *uaddr;
    if ((r = futex_addr(t, uaddrp, &uaddr)) < 0)
        return r;

    return host_futex_wait(t, uaddr, op, val, timeoutp ? &timeout : NULL);
}

// FUTEX_WAIT_BITSET is FUTEX_WAIT with two differences: the timeout is an
// absolute deadline, on CLOCK_MONOTONIC or (with FUTEX_CLOCK_REALTIME)
// CLOCK_REALTIME, and the bitset limits which FUTEX_WAKE_BITSET calls wake the
// waiter.
static long
futex_wait_bitset(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    lfiptr timeoutp, uint32_t bitset)
{
    long r;
    struct TimeSpec deadline;
    if (timeoutp && (r = read_timeout(t, timeoutp, &deadline)) < 0)
        return r;
    if (bitset == 0)
        return -LINUX_EINVAL;
    uint32_t *uaddr;
    if ((r = futex_addr(t, uaddrp, &uaddr)) < 0)
        return r;

    return host_futex_wait_bitset(t, uaddr, op, val,
        timeoutp ? &deadline : NULL, bitset);
}

long
full_sys_futex(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    uint64_t timeoutp, lfiptr uaddr2p, uint32_t val3)
{
    long r;
    uint32_t *uaddr, *uaddr2;
    uint32_t val2 = (uint32_t) timeoutp;
    switch (op & ~LINUX_FUTEX_PRIVATE_FLAG) {
    case LINUX_FUTEX_WAIT:
        return futex_wait(t, uaddrp, op, val, timeoutp);
    case LINUX_FUTEX_WAIT_BITSET:
    case LINUX_FUTEX_WAIT_BITSET | LINUX_FUTEX_CLOCK_REALTIME:
        return futex_wait_bitset(t, uaddrp, op, val, timeoutp, val3);
    case LINUX_FUTEX_WAKE:
        if ((r = futex_addr(t, uaddrp, &uaddr)) < 0)
            return r;
        return host_futex_wake(t, uaddr, op, val);
    case LINUX_FUTEX_REQUEUE:
        if ((int32_t) val < 0 || (int32_t) val2 < 0)
            return -LINUX_EINVAL;
        if ((r = futex_addr(t, uaddrp, &uaddr)) < 0)
            return r;
        if ((r = futex_addr(t, uaddr2p, &uaddr2)) < 0)
            return r;
        return host_futex_requeue(t, uaddr, op, val, val2, uaddr2);
    default:
        // Like Linux for an unknown operation. Also used for the operations
        // Linux has that are not supported here.
        LOG(t->proc->engine, "unsupported futex op %d", op);
        return -LINUX_ENOSYS;
    }
}
