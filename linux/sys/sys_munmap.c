#include "sys/sys.h"

int
sys_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t length)
{
    if (!ptrcheck(t, addrp))
        return -1;
    LOG(t->proc->engine, "sys_munmap(%lx, %ld)", addrp, length);
    return proc_unmap(t->proc, addrp, length);
}
