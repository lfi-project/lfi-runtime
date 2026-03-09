#include "sys/sys.h"

int
sys_fchdir(struct LFILinuxThread *t, int fd)
{
    if (fd < 0 || fd >= LINUX_NOFILE)
        return -LINUX_EBADF;
    LOCK_WITH_DEFER(&t->proc->fdtable.lk, lk_fdtable);
    if (t->proc->fdtable.fds[fd] == -1)
        return -LINUX_EBADF;
    char *dir = t->proc->fdtable.dirs[fd];
    if (!dir)
        return -LINUX_ENOTDIR;
    proc_chdir(t->proc, dir);
    return 0;
}
