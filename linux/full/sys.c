#include "full/sys/sys.h"

#include "sys.h"

bool
full_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret)
{
    // clang-format off
    switch (sysno) {
    FULL_SYS(mmap,
            full_sys_mmap(t, a0, a1, a2, a3, a4, a5))
    FULL_SYS(mprotect,
            full_sys_mprotect(t, a0, a1, a2))
    FULL_SYS(munmap,
            full_sys_munmap(t, a0, a1))
    }
    // clang-format on

    return false;
}
