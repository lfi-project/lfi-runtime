#include "cwalk.h"
#include "sys/sys.h"

#include <string.h>

int
sys_chdir(struct LFILinuxThread *t, lfiptr pathp)
{
    char *FREE_DEFER(path) = pathcopy(t, pathp);
    if (!path)
        return -LINUX_EINVAL;
    return proc_chdir(t->proc, path);
}
