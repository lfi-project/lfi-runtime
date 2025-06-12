#include "sys/sys.h"

#define LINUX_CLOCK_REALTIME  0
#define LINUX_CLOCK_MONOTONIC 1

static clockid_t
host_clockid(linux_clockid_t clockid)
{
    switch (clockid) {
    case LINUX_CLOCK_REALTIME:
        return CLOCK_REALTIME;
    case LINUX_CLOCK_MONOTONIC:
        return CLOCK_MONOTONIC;
    }
    assert(!"unreachable");
}

int
sys_clock_gettime(struct LFILinuxThread *t, linux_clockid_t clockid, lfiptr tp)
{
    if (clockid != LINUX_CLOCK_REALTIME && clockid != LINUX_CLOCK_MONOTONIC)
        return -LINUX_EINVAL;
    struct TimeSpec *box_ts = bufhost(t, tp, sizeof(struct TimeSpec),
        alignof(struct TimeSpec));
    if (!box_ts)
        return -LINUX_EFAULT;
    struct timespec ts;
    int err = clock_gettime(host_clockid(clockid), &ts);
    if (err < 0)
        return host_err(errno);
    box_ts->sec = ts.tv_sec;
    box_ts->nsec = ts.tv_nsec;
    return 0;
}
