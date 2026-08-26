#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_write(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size)
{
    if (size == 0)
        return 0;
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;
    return HOST_ERR(ssize_t, write(f.kfd, buf, size));
}
