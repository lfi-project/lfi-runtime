#include "sys/sys.h"

#include <unistd.h>

int
sys_fchown(struct LFILinuxThread *t, int fd, linux_uid_t owner,
    linux_gid_t group)
{
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    return HOST_ERR(int, fchown(kfd, owner, group));
}
