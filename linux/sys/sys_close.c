#include "sys/sys.h"

int
sys_close(struct LFILinuxThread *t, int fd)
{
    if (!fdclose(&t->proc->fdtable, fd))
        return -LINUX_EBADF;
    return 0;
}
