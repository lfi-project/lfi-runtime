#include "sys/sys.h"

int
sys_chmod(struct LFILinuxThread *t, lfiptr pathp, linux_mode_t mode)
{
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_EINVAL;
    free(path);
    return HOST_ERR(int, chmod(host_path, mode));
}
