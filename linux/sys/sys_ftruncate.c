#include "sys/sys.h"

#include <unistd.h>

int
sys_ftruncate(struct LFILinuxThread *t, int fd, off_t length)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, ftruncate(kfd, length));
}
