#include "sys/sys.h"

int
sys_faccessat(struct LFILinuxThread *t, int dirfd, uintptr_t pathp, int mode)
{
    assert(!"unimplemented");
}
