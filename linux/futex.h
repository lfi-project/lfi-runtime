#pragma once

#include <stdint.h>

struct LFILinuxThread;
struct Futexes;
struct TimeSpec;

struct Futexes *
futexes_new(void);

void
futexes_free(struct Futexes *fxs);

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout);

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val);

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2);
