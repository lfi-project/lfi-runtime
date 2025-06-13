#include "sys/sys.h"

#include <unistd.h>

static int
accessmode(int mode)
{
    if (mode == LINUX_F_OK)
        return F_OK;
    return ((mode & LINUX_R_OK) ? R_OK : 0) |
        ((mode & LINUX_W_OK) ? W_OK : 0) |
        ((mode & LINUX_X_OK) ? X_OK : 0);
}

int
sys_faccessat(struct LFILinuxThread *t, int dirfd, uintptr_t pathp, int mode)
{
    if (dirfd != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_EINVAL;
    free(path);
    return HOST_ERR(int, access(host_path, accessmode(mode)));
}
