#include "sys/sys.h"

ssize_t
sys_readlinkat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, lfiptr bufp,
    size_t size)
{
    assert(!"unimplemented");
}
