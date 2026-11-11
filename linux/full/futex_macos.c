// macOS futex backend built on top of os_sync_wait_on_address /
// os_sync_wake_by_address_*, available since macOS 14.4.

#include "full/futex.h"
#include "linux.h"

#include <errno.h>
#include <limits.h>
#include <os/os_sync_wait_on_address.h>
#include <stdint.h>
#include <time.h>

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

// The bitset is ignored, which is exact because every wake the runtime
// supports matches every non-zero bitset. The deadline is converted to a
// relative timeout using the clock that op selects.
long
host_futex_wait_bitset(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, struct TimeSpec *deadline, uint32_t bitset)
{
    (void) bitset;
    if (!deadline)
        return host_futex_wait(t, uaddr, op, val, NULL);

    struct timespec now;
    clockid_t clockid = (op & LINUX_FUTEX_CLOCK_REALTIME) ? CLOCK_REALTIME :
                                                            CLOCK_MONOTONIC;
    if (clock_gettime(clockid, &now) < 0)
        return err_to_linux(errno);

    int64_t sec = (int64_t) deadline->sec - (int64_t) now.tv_sec;
    int64_t nsec = (int64_t) deadline->nsec - (int64_t) now.tv_nsec;
    if (nsec < 0) {
        nsec += 1000000000;
        sec--;
    }
    if (sec < 0 || (sec == 0 && nsec == 0)) {
        // The deadline has passed, but like Linux the word is still compared.
        // The host compares it, since the sandbox may have unmapped it. A wait
        // that returns before its timeout either found a different value or
        // was woken, and both are reported as EAGAIN.
        struct TimeSpec shortest = { 0, 1 };
        long r = host_futex_wait(t, uaddr, op, val, &shortest);
        return r == 0 ? -LINUX_EAGAIN : r;
    }
    // A timeout in nanoseconds overflows after about 584 years. Anything
    // beyond a century waits without one.
    if (sec > 100LL * 365 * 24 * 60 * 60)
        return host_futex_wait(t, uaddr, op, val, NULL);

    struct TimeSpec timeout = { (uint64_t) sec, (uint64_t) nsec };
    return host_futex_wait(t, uaddr, op, val, &timeout);
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
