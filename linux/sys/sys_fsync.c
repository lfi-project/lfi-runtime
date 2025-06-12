#include "sys/sys.h"

#include <unistd.h>

int
sys_fsync(struct LFILinuxThread *t, int fd)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fsync(kfd));
}
