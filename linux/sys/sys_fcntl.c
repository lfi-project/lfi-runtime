#include "sys/sys.h"

int
sys_fcntl(struct LFILinuxThread *t, int fd, int cmd, uintptr_t va0,
    uintptr_t va1, uintptr_t va2, uintptr_t va3)
{
    assert(!"unimplemented");
}
