#include "sys/sys.h"

#include <unistd.h>

int
sys_renameat2(struct LFILinuxThread *t, int olddir, lfiptr oldpathp, int newdir,
    lfiptr newpathp, unsigned int flags)
{
  if (flags != 0){
    return -LINUX_EINVAL;
  }

  return sys_renameat(t, olddir, oldpathp, newdir, newpathp);
}
