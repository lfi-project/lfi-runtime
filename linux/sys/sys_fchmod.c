#include "sys/sys.h"

#include <unistd.h>

int
sys_fchmod(struct LFILinuxThread *t, int fd, linux_mode_t mode)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fchmod(f.kfd, mode));
}
