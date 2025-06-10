#include "sys/sys.h"

int
sys_set_tid_address(struct LFILinuxThread *t, lfiptr ctid)
{
    if (!ptrcheck(t, ctid))
        return -LINUX_EINVAL;
    t->ctidp = ctid;
    return t->ctidp;
}
