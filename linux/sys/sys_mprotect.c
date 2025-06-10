#include "sys/sys.h"
#include "lock.h"

int
sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot)
{
    if (!ptrcheck(t, addrp))
        return -1;
    LOCK_WITH_DEFER(&t->proc->lk_box, lk_box);
    return lfi_box_mprotect(t->proc->box, addrp, length, prot);
}
