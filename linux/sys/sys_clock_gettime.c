#include "sys/sys.h"

#define LINUX_CLOCK_REALTIME  0
#define LINUX_CLOCK_MONOTONIC 1

int
sys_clock_gettime(struct LFILinuxThread *t, linux_clockid_t clockid, lfiptr tp)
{
    assert(!"unimplemented");
}
