// Futex backend for hosts with no native futex or wait-on-address API. Not
// implemented: every operation fails with ENOSYS.

#include "full/futex.h"
#include "linux.h"

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    (void) t, (void) uaddr, (void) op, (void) val, (void) timeout;
    ERROR("futex_wait is not supported on this host");
    return -LINUX_ENOSYS;
}

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    (void) t, (void) uaddr, (void) op, (void) val;
    ERROR("futex_wake is not supported on this host");
    return -LINUX_ENOSYS;
}

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    (void) t, (void) uaddr, (void) op, (void) val, (void) val2, (void) uaddr2;
    ERROR("futex_requeue is not supported on this host");
    return -LINUX_ENOSYS;
}
