#include "config.h"
#include "linux.h"
#include "arch_sys.h"

#include <assert.h>

#define SYS(SYSNO, expr) \
case LINUX_SYS_##SYSNO:  \
    r = expr;            \
    break;

uintptr_t
syshandle(struct LFILinuxThread* t, uintptr_t sysno, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
    assert(!"unimplemented");
}
