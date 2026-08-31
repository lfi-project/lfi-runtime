#include "align.h"
#include "lock.h"
#include "sys/sys.h"

int
sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot)
{
    if (length == 0)
        return -LINUX_EINVAL;
    if (!ptrcheck(t, addrp))
        return -1;
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    length = ceilp(length, pagesize);
    return lfi_box_mprotect(t->proc->box, addrp, length, prot);
}
