#include "sys/sys.h"

#include <unistd.h>

off_t
sys_lseek(struct LFILinuxThread *t, int fd, off_t offset, int whence)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(off_t, lseek(f.kfd, offset, whence));
}
