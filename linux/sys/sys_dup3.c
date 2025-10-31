#include "sys/sys.h"

#include <unistd.h>

int
sys_dup3(struct LFILinuxThread *t, int oldfd, int newfd, int flags)
{
    if (flags != 0)
        return -LINUX_EINVAL;
    return fddup2(&t->proc->fdtable, oldfd, newfd);
}
