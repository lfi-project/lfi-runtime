#include "sys/sys.h"

int
sys_gettid(struct LFILinuxThread *t)
{
    return t->tid;
}
