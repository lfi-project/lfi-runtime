#include "sys/sys.h"

int
sys_newfstatat(struct LFILinuxThread *t, int dirfd, lfiptr pathp,
    lfiptr statbufp, int flags)
{
    assert(!"unimplemented");
}
