#include "sys/sys.h"

int
sys_mkdirat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    linux_mode_t mode)
{
    assert(!"unimplemented");
}
