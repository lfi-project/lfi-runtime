#include "sys/sys.h"

#include <sched.h>

int
sys_sched_yield(struct LFILinuxThread *t)
{
    return sched_yield();
}
