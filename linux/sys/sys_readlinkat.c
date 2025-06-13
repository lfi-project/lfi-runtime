#include "sys/sys.h"

#include <unistd.h>

ssize_t
sys_readlinkat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, lfiptr bufp,
    size_t size)
{
    if (dirfd != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_EINVAL;
    free(path);

    char *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;

    return HOST_ERR(ssize_t, readlink(host_path, buf, size));
}
