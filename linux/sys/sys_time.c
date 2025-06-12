#include "sys/sys.h"

linux_time_t
sys_time(struct LFILinuxThread *t, lfiptr tlocp)
{
    // TODO: sys_time: currently we require tlocp to be NULL.
    if (tlocp != 0)
        return -LINUX_EINVAL;
    return HOST_ERR(linux_time_t, time(NULL));
}
