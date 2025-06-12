#include "sys/sys.h"

#include <unistd.h>

int
sys_fchmod(struct LFILinuxThread *t, int fd, linux_mode_t mode)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fchmod(fd, mode));
}
