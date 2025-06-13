#include "sys/sys.h"

int
sys_mkdirat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    linux_mode_t mode)
{
    if (dirfd != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_EINVAL;
    free(path);
    return HOST_ERR(int, mkdir(host_path, mode));
}
