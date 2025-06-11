#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_read(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size)
{
    if (size == 0)
        return 0;
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;
    return HOST_ERR(ssize_t, read(kfd, buf, size));
}
