#include "align.h"
#include "sys/sys.h"

#include <string.h>

ssize_t
sys_getcwd(struct LFILinuxThread *t, lfiptr bufp, size_t size)
{
    if (size == 0)
        return 0;
    uint8_t *buf = bufhost(t, bufp, size, 1);
    if (!buf)
        return -LINUX_EINVAL;
    LOCK_WITH_DEFER(&t->proc->cwd.lk, lk_cwd);
    size_t len = MIN(size,
        strnlen(t->proc->cwd.path, sizeof(t->proc->cwd.path) - 1) + 1);
    memcpy(buf, t->proc->cwd.path, len);
    buf[len - 1] = 0;
    return len;
}
