#include "sys/sys.h"

ssize_t
sys_getrandom(struct LFILinuxThread *t, lfiptr bufp, size_t buflen,
    unsigned int flags)
{
    assert(!"unimplemented");
}
