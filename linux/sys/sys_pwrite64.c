#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_pwrite64(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size,
    off_t offset)
{
    if (size == 0)
        return 0;
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EFAULT;
    return HOST_ERR(ssize_t, pwrite(kfd, buf, size, offset));
}
