#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_pread64(struct LFILinuxThread *t, int fd, lfiptr bufp, size_t size,
    ssize_t offset)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EFAULT;
    ssize_t orig = lseek(kfd, 0, SEEK_CUR);
    if (orig < 0)
        return host_err(orig);
    lseek(kfd, offset, SEEK_SET);
    ssize_t n = read(kfd, buf, size);
    lseek(kfd, orig, SEEK_SET);
    return n;
}
