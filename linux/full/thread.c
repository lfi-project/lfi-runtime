#include "full.h"

#include <stdlib.h>

bool
full_thread_init(struct LFILinuxThread *t)
{
    struct LFIFullThread *full = calloc(1, sizeof(*full));
    if (!full)
        return false;
    t->full = full;
    return true;
}

void
full_thread_free(struct LFILinuxThread *t)
{
    free(t->full);
    t->full = NULL;
}
