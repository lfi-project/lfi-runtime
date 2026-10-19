#pragma once

#include "proc.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef LINUX_FULL

#include "full/full.h"

#else

static inline bool
full_proc_init(struct LFILinuxProc *proc)
{
    (void) proc;
    return true;
}

static inline void
full_proc_free(struct LFILinuxProc *proc)
{
    (void) proc;
}

static inline bool
full_thread_init(struct LFILinuxThread *t)
{
    (void) t;
    return true;
}

static inline void
full_thread_free(struct LFILinuxThread *t)
{
    (void) t;
}

static inline bool
full_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret)
{
    (void) t, (void) sysno, (void) a0, (void) a1, (void) a2, (void) a3,
        (void) a4, (void) a5, (void) o_ret;
    return false;
}

#endif
