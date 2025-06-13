#include "sys/sys.h"

#include <unistd.h>

int
sys_renameat(struct LFILinuxThread *t, int olddir, lfiptr oldpathp, int newdir,
    lfiptr newpathp)
{
    if (olddir != LINUX_AT_FDCWD || newdir != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    char old_host_path[FILENAME_MAX];
    char *oldpath = pathcopyresolve(t, oldpathp, old_host_path, sizeof(old_host_path));
    if (!oldpath)
        return -LINUX_EINVAL;
    free(oldpath);
    char new_host_path[FILENAME_MAX];
    char *newpath = pathcopyresolve(t, newpathp, new_host_path, sizeof(new_host_path));
    if (!newpath)
        return -LINUX_EINVAL;
    free(newpath);
    return HOST_ERR(int, rename(old_host_path, new_host_path));
}
