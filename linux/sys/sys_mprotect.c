#include "lock.h"
#include "sys/sys.h"

int
sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length, int prot)
{
    if (!ptrcheck(t, addrp))
        return -1;
    return lfi_box_mprotect(t->proc->box, addrp, length, prot);
}
