#include "sys/sys.h"

#include <unistd.h>

int
sys_dup(struct LFILinuxThread *t, int oldfd)
{
    return fddup2(&t->proc->fdtable, oldfd, -1);
}
