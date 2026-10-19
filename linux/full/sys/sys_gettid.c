#include "full/sys/sys.h"

int
full_sys_gettid(struct LFILinuxThread *t)
{
    return t->full->tid;
}
