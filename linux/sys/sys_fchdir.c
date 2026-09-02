#include "sys/sys.h"

#include <string.h>

int
sys_fchdir(struct LFILinuxThread *t, int fd)
{
    if (fd < 0 || fd >= LINUX_NOFILE)
        return -LINUX_EBADF;

    char dir[FILENAME_MAX];
    {
        LOCK_WITH_DEFER(&t->proc->fdtable.lk, lk_fdtable);
        if (t->proc->fdtable.fds[fd] == -1)
            return -LINUX_EBADF;
        const char *d = t->proc->fdtable.dirs[fd];
        if (!d)
            return -LINUX_ENOTDIR;
        strncpy(dir, d, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = 0;
    }

    return proc_chdir(t->proc, dir);
}
