#include "sys/sys.h"

#include <unistd.h>

int
sys_fsync(struct LFILinuxThread *t, int fd)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fsync(f.kfd));
}
