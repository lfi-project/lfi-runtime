#include "futex.h"
#include "linux.h"

#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

struct Futexes *
futexes_new(void)
{
    return NULL;
}

void
futexes_free(struct Futexes *fxs)
{
    (void) fxs;
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
    return r;
}

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    (void) t;
    return syscall(SYS_futex, uaddr, op, val);
}

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    (void) t;
    return syscall(SYS_futex, uaddr, op, val, val2, uaddr2);
}
