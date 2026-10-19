#pragma once

// Host futex backends for full_sys_futex. Exactly one futex_*.c is built,
// depending on the host (see meson.build). Each returns a Linux result: the
// syscall's value, or a negated Linux errno.

#include <stdint.h>

struct LFILinuxThread;

// Linux struct timespec on 64-bit targets.
struct TimeSpec {
    uint64_t sec;
    uint64_t nsec;
};

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout);

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val);

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2);
