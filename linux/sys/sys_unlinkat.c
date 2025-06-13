#include "sys/sys.h"

#include <unistd.h>

int
sys_unlinkat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, int flags)
{
    if (dirfd != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    if (flags != 0)
        return -LINUX_EINVAL;
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_EINVAL;
    free(path);
    return HOST_ERR(int, unlink(host_path));
}
