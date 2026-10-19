#include "full.h"

#include <stdlib.h>

bool
full_proc_init(struct LFILinuxProc *proc)
{
    struct LFIFullProc *full = calloc(1, sizeof(*full));
    if (!full)
        return false;
    proc->full = full;
    return true;
}

void
full_proc_free(struct LFILinuxProc *proc)
{
    free(proc->full);
    proc->full = NULL;
}
