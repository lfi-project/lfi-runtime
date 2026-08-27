#include "sys/sys.h"

int
sys_set_tid_address(struct LFILinuxThread *t, lfiptr ctid)
{
    if (!bufcheck(t, ctid, sizeof(int), alignof(int)))
        return -LINUX_EINVAL;
    t->ctidp = ctid;
    return t->tid;
}
