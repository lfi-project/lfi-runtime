#include "sys/sys.h"

#include <sys/prctl.h>

static int
pr_set_name(struct LFILinuxThread *t, lfiptr namep)
{
    char* name = (char*) bufhost(t, namep, 16, 1);
    if (!name)
        return -LINUX_EINVAL;
    return prctl(PR_SET_NAME, name);
}

int
sys_prctl(struct LFILinuxThread *t, int op, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    switch (op) {
    case LINUX_PR_SET_NAME:
        return pr_set_name(t, arg2);
    default:
        LOG(t->proc->engine, "unknown prctl op %d", op);
        return -LINUX_EINVAL;
    }
}
