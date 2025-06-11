#include "sys/sys.h"

ssize_t
sys_getdents64(struct LFILinuxThread *t, int fd, lfiptr dirp, size_t count)
{
    assert(!"unimplemented");
}
