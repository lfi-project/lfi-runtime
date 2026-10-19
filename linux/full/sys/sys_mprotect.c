#include "align.h"
#include "full/sys/sys.h"

#include <errno.h>

int
full_sys_mprotect(struct LFILinuxThread *t, lfiptr addrp, size_t length,
    int prot)
{
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    if (addrp % pagesize != 0)
        return -LINUX_EINVAL;
    if (length == 0)
        return 0;
    if (length > SIZE_MAX - (pagesize - 1))
        return -LINUX_ENOMEM;
    length = ceilp(length, pagesize);
    // Like Linux, PROT_SEM is accepted and ignored. Anything else outside
    // read/write/exec is rejected, including PROT_GROWSDOWN/PROT_GROWSUP, since
    // there are no grows-down mappings.
    const int valid_prot = LFI_PROT_READ | LFI_PROT_WRITE | LFI_PROT_EXEC |
        LINUX_PROT_SEM;
    if ((prot & ~valid_prot) != 0)
        return -LINUX_EINVAL;
    prot &= ~LINUX_PROT_SEM;
    if (!lfi_box_bufvalid(t->proc->box, addrp, length))
        return -LINUX_ENOMEM;
    if (prot & LFI_PROT_EXEC)
        return -LINUX_EACCES;

    // Like Linux, refuse if any page in the range is not mapped (the brk
    // reservation counts as unmapped). Unmapped sandbox pages are reserved
    // PROT_NONE memory, and changing their protection would make them
    // accessible without a mapping. The check and the protection change happen
    // under one lock, so a concurrent munmap cannot slip in between.
    int r = lfi_box_mprotect_mapped(t->proc->box, addrp, length, prot);
    if (r == -ENOMEM)
        return -LINUX_ENOMEM;
    if (r == -EACCES)
        return -LINUX_EACCES;
    if (r != 0)
        return -LINUX_EINVAL;
    return 0;
}
