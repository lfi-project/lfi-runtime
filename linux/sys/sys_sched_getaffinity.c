#include "align.h"
#include "sys/sys.h"

#include <unistd.h>

static unsigned
host_cpucount(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

int
sys_sched_getaffinity(struct LFILinuxThread *t, int32_t pid,
    uint64_t cpusetsize, lfiptr maskaddr)
{
    unsigned count = host_cpucount();
    unsigned rc = ceilp(count, 64) / 8;
    if (cpusetsize < rc)
        return -LINUX_EINVAL;
    if (!bufcheck(t, maskaddr, rc, 1))
        return -LINUX_EINVAL;
    count = MIN(count, rc * 8);
    uint8_t *mask = calloc(1, rc);
    if (!mask)
        return -LINUX_ENOMEM;
    for (unsigned i = 0; i < count; i++) {
        mask[i / 8] |= 1 << (i % 8);
    }
    copyin(t, maskaddr, mask, rc);
    free(mask);
    return rc;
}
