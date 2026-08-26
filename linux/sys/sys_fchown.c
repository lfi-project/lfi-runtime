#include "sys/sys.h"

#include <unistd.h>

int
sys_fchown(struct LFILinuxThread *t, int fd, linux_uid_t owner,
    linux_gid_t group)
{
    FD_ACQUIRE(f, &t->proc->fdtable, fd, NULL, NULL);
    if (f.kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fchown(f.kfd, owner, group));
}
