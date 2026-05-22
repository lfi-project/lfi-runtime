// macOS futex backend built on top of os_sync_wait_on_address /
// os_sync_wake_by_address_*, available since macOS 14.4.

#include "futex.h"
#include "linux.h"

#include <errno.h>
#include <limits.h>
#include <os/os_sync_wait_on_address.h>
#include <stdint.h>

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

static long
err_to_linux(int e)
{
    switch (e) {
    case ETIMEDOUT:
        return -LINUX_ETIMEDOUT;
    case EAGAIN:
        return -LINUX_EAGAIN;
    case EFAULT:
        return -LINUX_EFAULT;
    case EINTR:
        return -LINUX_EAGAIN;
    default:
        return -LINUX_EINVAL;
    }
}

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    (void) t;
    (void) op;
    int r;
    if (!timeout) {
        r = os_sync_wait_on_address(uaddr, (uint64_t) val, sizeof(uint32_t),
            OS_SYNC_WAIT_ON_ADDRESS_NONE);
    } else {
        uint64_t ns = (uint64_t) timeout->sec * 1000000000ULL +
            (uint64_t) timeout->nsec;
        r = os_sync_wait_on_address_with_timeout(uaddr, (uint64_t) val,
            sizeof(uint32_t), OS_SYNC_WAIT_ON_ADDRESS_NONE,
            OS_CLOCK_MACH_ABSOLUTE_TIME, ns);
    }
    if (r < 0)
        return err_to_linux(errno);
    return 0;
}

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    (void) t;
    (void) op;
    // INT_MAX is the standard "wake everyone" sentinel; -1 (cast to uint32_t)
    // is how musl encodes the same in some paths.
    if (val == 0xFFFFFFFFu || val >= (uint32_t) INT_MAX) {
        int r = os_sync_wake_by_address_all(uaddr, sizeof(uint32_t),
            OS_SYNC_WAKE_BY_ADDRESS_NONE);
        if (r < 0)
            return errno == ENOENT ? 0 : err_to_linux(errno);
        // We don't get an exact count back; return INT_MAX so callers that
        // treat the result as "many" do the right thing.
        return INT_MAX;
    }
    long woken = 0;
    for (uint32_t i = 0; i < val; i++) {
        int r = os_sync_wake_by_address_any(uaddr, sizeof(uint32_t),
            OS_SYNC_WAKE_BY_ADDRESS_NONE);
        if (r < 0) {
            if (errno == ENOENT)
                break;
            return err_to_linux(errno);
        }
        woken++;
    }
    return woken;
}

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    (void) val2;
    (void) uaddr2;
    // No direct equivalent on macOS. Wake `val` waiters from uaddr and let
    // libc re-park them on uaddr2 if needed; this is what pthread_cond paths
    // expect when CMP_REQUEUE returns fewer requeued than asked for.
    return host_futex_wake(t, uaddr, op, val);
}
