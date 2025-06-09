#include "sys/sys.h"

uintptr_t
sys_ignore(struct LFILinuxProc *p, const char *name)
{
    LOG(p->engine, "%s: ignored", name);
    return 0;
}

uintptr_t
sys_nosys(struct LFILinuxProc *p, const char *name)
{
    LOG(p->engine, "%s: unsupported (ENOSYS)", name);
    return -LINUX_ENOSYS;
}
