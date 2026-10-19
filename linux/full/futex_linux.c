#include "full/futex.h"

#include <errno.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// The host is Linux, so its errno values are the Linux ones.
static long
result(long r)
{
    return r == -1 ? -errno : r;
}

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    (void) t;
    long r;
    if (!timeout) {
        r = syscall(SYS_futex, uaddr, op, val, NULL);
    } else {
        struct timespec k_timeout = { 0 };
        k_timeout.tv_sec = timeout->sec;
        k_timeout.tv_nsec = timeout->nsec;
        r = syscall(SYS_futex, uaddr, op, val, &k_timeout);
    }
    return result(r);
}

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    (void) t;
    return result(syscall(SYS_futex, uaddr, op, val));
}

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    (void) t;
    return result(syscall(SYS_futex, uaddr, op, val, val2, uaddr2));
}
