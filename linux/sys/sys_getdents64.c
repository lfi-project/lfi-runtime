#include "sys/sys.h"

ssize_t
sys_getdents64(struct LFILinuxThread *t, int fd, lfiptr dirp, size_t count)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, dirp, count, alignof(struct Dirent));
    if (!buf)
        return -LINUX_EINVAL;
    return host_getdents64(f.kfd, buf, count);
}
