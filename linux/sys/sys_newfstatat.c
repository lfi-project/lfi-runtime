#include "sys/sys.h"

int
sys_newfstatat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    lfiptr statbufp, int flags)
{
    struct Stat *stat_ = bufhost(t, statbufp, sizeof(struct Stat),
        alignof(struct Stat));
    if (!stat_)
        return -LINUX_EINVAL;
    if ((flags & LINUX_AT_EMPTY_PATH) == 0) {
        char host_path[FILENAME_MAX];
        char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
        if (!path)
            return -LINUX_EINVAL;
        free(path);
        if (dirfd != LINUX_AT_FDCWD)
            return -LINUX_EBADF;
        return host_fstatat(AT_FDCWD, host_path, stat_, flags);
    }
    int kfd = fdget(&t->proc->fdtable, dirfd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return host_fstatat(kfd, "", stat_, 0);
}
