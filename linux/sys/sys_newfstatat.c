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
    FD_ACQUIRE(f, &t->proc->fdtable, dirfd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return host_fstatat(f.kfd, "", stat_, LINUX_AT_EMPTY_PATH);
}
