#include "sys/sys.h"

#include <unistd.h>

#ifdef HAVE_SYS_FUTEX
#include <sys/syscall.h>

static long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    long r;
    if (!timeout) {
        r = syscall(SYS_futex, uaddr, op, val, NULL);
    } else {
        struct timespec k_timeout = { 0 };
        k_timeout.tv_sec = timeout->sec;
        k_timeout.tv_nsec = timeout->nsec;
        r = syscall(SYS_futex, uaddr, op, val, &k_timeout);
    }
    return r;
}

static long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    return syscall(SYS_futex, uaddr, op, val);
}

static long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    return syscall(SYS_futex, uaddr, op, val, val2, uaddr2);
}
#else
static long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    ERROR("futex_wait not supported");
    return LINUX_ENOSYS;
}

static long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    ERROR("futex_wake not supported");
    return LINUX_ENOSYS;
}

static long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    ERROR("futex_requeue not supported");
    return LINUX_ENOSYS;
}
#endif

static long
futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    uintptr_t timeoutp)
{
    struct TimeSpec *timeout = NULL;
    if (timeoutp) {
        timeout = (struct TimeSpec *) bufhost(t, timeoutp,
            sizeof(struct TimeSpec), alignof(struct TimeSpec));
        if (!timeout)
            return -LINUX_EFAULT;
    }

    return host_futex_wait(t, uaddr, op, val, timeout);
}

static long
futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    return host_futex_wake(t, uaddr, op, val);
}

static long
futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    uint32_t val2, uint32_t *uaddr2)
{
    return host_futex_requeue(t, uaddr, op, val, val2, uaddr2);
}

long
sys_futex(struct LFILinuxThread *t, lfiptr uaddrp, int op, uint32_t val,
    uint64_t timeoutp, lfiptr uaddr2p, uint32_t val3)
{
    if (uaddrp & 3)
        return -LINUX_EINVAL;

    uint32_t *uaddr = bufhost(t, uaddrp, sizeof(uint32_t), alignof(uint32_t));
    if (!uaddr)
        return -LINUX_EINVAL;

    uint32_t val2 = (uint32_t) timeoutp;
    uint32_t *uaddr2;
    switch (op & ~LINUX_FUTEX_PRIVATE_FLAG) {
    case LINUX_FUTEX_WAIT:
        return futex_wait(t, uaddr, op, val, timeoutp);
    case LINUX_FUTEX_WAKE:
        return futex_wake(t, uaddr, op, val);
    case LINUX_FUTEX_REQUEUE:
        uaddr2 = bufhost(t, uaddr2p, sizeof(uint32_t), alignof(uint32_t));
        if (!uaddr2)
            return -LINUX_EINVAL;
        return futex_requeue(t, uaddr, op, val, val2, uaddr2);
    default:
        LOG(t->proc->engine, "invalid futex op %d", op);
        return -LINUX_EINVAL;
    }
}
