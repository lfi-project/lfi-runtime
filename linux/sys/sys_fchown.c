#include "sys/sys.h"

int
sys_fchown(struct LFILinuxThread *t, int fd, linux_uid_t owner,
    linux_gid_t group)
{
    assert(!"unimplemented");
}
