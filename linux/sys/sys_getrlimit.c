#include "sys/sys.h"

#define LINUX_RLIMIT_CPU     0
#define LINUX_RLIMIT_FSIZE   1
#define LINUX_RLIMIT_DATA    2
#define LINUX_RLIMIT_STACK   3
#define LINUX_RLIMIT_CORE    4
#define LINUX_RLIMIT_RSS     5
#define LINUX_RLIMIT_NPROC   6
#define LINUX_RLIMIT_NOFILE  7
#define LINUX_RLIMIT_MEMLOCK 8
#define LINUX_RLIMIT_AS      9

struct RLimit {
    uint64_t cur;
    uint64_t max;
};

ssize_t
sys_getrlimit(struct LFILinuxThread *t, int resource, lfiptr rlimp)
{
    struct RLimit *rlim = bufhost(t, rlimp, sizeof(struct RLimit), alignof(struct RLimit));
    if (!rlim)
        return -LINUX_EINVAL;

    switch (resource) {
    case LINUX_RLIMIT_STACK:
        rlim->max = t->stack_size;
        rlim->cur = t->stack_size;
        break;
    case LINUX_RLIMIT_AS:
        rlim->max = t->proc->box_info.size;
        rlim->cur = t->proc->box_info.size;
        break;
    case LINUX_RLIMIT_CORE:
        rlim->max = 0;
        rlim->cur = 0;
        break;
    case LINUX_RLIMIT_RSS:
        rlim->max = t->proc->box_info.size;
        rlim->cur = t->proc->box_info.size;
        break;
    case LINUX_RLIMIT_DATA:
        rlim->max = t->proc->box_info.size;
        rlim->cur = t->proc->box_info.size;
        break;
    case LINUX_RLIMIT_NOFILE:
        rlim->max = LINUX_NOFILE;
        rlim->cur = LINUX_NOFILE;
        break;
    default:
        return -LINUX_EINVAL;
    }

    return 0;
}
