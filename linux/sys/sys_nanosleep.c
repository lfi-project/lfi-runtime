#include "sys/sys.h"

int
sys_nanosleep(struct LFILinuxThread *t, lfiptr reqp, lfiptr remp)
{
    struct TimeSpec *box_req = bufhost(t, reqp, sizeof(struct TimeSpec),
        alignof(struct TimeSpec));
    if (!box_req)
        return -LINUX_EINVAL;
    struct timespec req, rem;
    req.tv_sec = box_req->sec;
    req.tv_nsec = box_req->nsec;
    int r = nanosleep(&req, &rem);
    if (r < 0)
        return host_err(errno);

    if (remp) {
        uint8_t *remu = bufhost(t, remp, sizeof(struct TimeSpec),
            alignof(struct TimeSpec));
        if (!remu)
            return -LINUX_EFAULT;
        struct TimeSpec *box_rem = (struct TimeSpec *) remu;
        box_rem->sec = rem.tv_sec;
        box_rem->nsec = rem.tv_nsec;
    }

    return r;
}
