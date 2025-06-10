#include "sys/sys.h"

ssize_t
sys_write(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size)
{
    if (size == 0)
        return 0;
    struct FDFile *FD_DEFER(f) = fdget(&t->proc->fdtable, fd);
    if (!f)
        return -LINUX_EBADF;
    if (!f->write)
        return -LINUX_EPERM;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;
    return f->write(f->dev, buf, size);
}
