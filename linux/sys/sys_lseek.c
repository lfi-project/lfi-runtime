#include "sys/sys.h"

#include <unistd.h>

off_t
sys_lseek(struct LFILinuxThread *t, int fd, off_t offset, int whence)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(off_t, lseek(kfd, offset, whence));
}
