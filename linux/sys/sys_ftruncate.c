#include "sys/sys.h"

#include <unistd.h>

int
sys_ftruncate(struct LFILinuxThread *t, int fd, off_t length)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, ftruncate(f.kfd, length));
}
