#include "full/sys/sys.h"

int
full_sys_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t length)
{
    if (!ptrcheck(t, addrp))
        return -LINUX_EINVAL;
    LOG(t->proc->engine, "sys_munmap(%lx, %ld)", addrp, length);
    // Runtime-owned (pinned) mappings cannot be unmapped.
    return lfi_box_munmap_unpinned(t->proc->box, addrp, length);
}
