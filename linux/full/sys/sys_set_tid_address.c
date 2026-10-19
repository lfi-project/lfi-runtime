#include "full/sys/sys.h"

#include <stdalign.h>

int
full_sys_set_tid_address(struct LFILinuxThread *t, lfiptr ctidp)
{
    // Like Linux, a null pointer clears the address.
    if (ctidp && !bufcheck(t, ctidp, sizeof(int), alignof(int)))
        return -LINUX_EINVAL;
    t->full->ctidp = ctidp;
    return t->full->tid;
}
