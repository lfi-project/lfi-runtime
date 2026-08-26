#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_read(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size)
{
    if (size == 0)
        return 0;
    // Acquire the fd, its flags, and its directory status in one atomic step
    // so they cannot change (or the fd be closed) between the checks and the
    // read below.
    int flags;
    bool isdir;
    FD_ACQUIRE(f, &t->proc->fdtable, fd, &flags, &isdir);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    if (flags & LINUX_O_PATH)
        return -LINUX_EBADF;
    if (isdir)
        return -LINUX_EISDIR;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;
    return HOST_ERR(ssize_t, read(f.kfd, buf, size));
}
