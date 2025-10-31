#include "sys/sys.h"

#define LINUX_CLOCK_REALTIME           0
#define LINUX_CLOCK_MONOTONIC          1
#define LINUX_CLOCK_PROCESS_CPUTIME_ID 2

static bool
host_clockid(linux_clockid_t clockid, clockid_t *out_clockid)
{
    switch (clockid) {
    case LINUX_CLOCK_REALTIME:
        *out_clockid = CLOCK_REALTIME;
        return true;
    case LINUX_CLOCK_MONOTONIC:
        *out_clockid = CLOCK_MONOTONIC;
        return true;
#ifdef CLOCK_PROCESS_CPUTIME_ID
    case LINUX_CLOCK_PROCESS_CPUTIME_ID:
        *out_clockid = CLOCK_PROCESS_CPUTIME_ID;
        return true;
#endif
    }
    return false;
}

int
sys_clock_gettime(struct LFILinuxThread *t, linux_clockid_t clockid, lfiptr tp)
{
    clockid_t h_clockid;
    if (!host_clockid(clockid, &h_clockid))
        return -LINUX_EINVAL;
    struct TimeSpec *box_ts = bufhost(t, tp, sizeof(struct TimeSpec),
        alignof(struct TimeSpec));
    if (!box_ts)
        return -LINUX_EFAULT;
    struct timespec ts;
    int err = clock_gettime(h_clockid, &ts);
    if (err < 0)
        return host_err(errno);
    box_ts->sec = ts.tv_sec;
    box_ts->nsec = ts.tv_nsec;
    return 0;
}
