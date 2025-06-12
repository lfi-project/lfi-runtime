#include "sys/sys.h"

ssize_t
sys_getdents64(struct LFILinuxThread *t, int fd, lfiptr dirp, size_t count)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, dirp, count, alignof(struct Dirent));
    if (!buf)
        return -LINUX_EINVAL;
    return host_getdents64(kfd, buf, count);
}
