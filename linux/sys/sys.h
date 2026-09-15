#pragma once

#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"
#include "proc.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

// Returns true if the pointer is valid for the sandbox.
static inline bool
ptrcheck(struct LFILinuxThread *t, lfiptr p)
{
    return lfi_box_ptrvalid(t->proc->box, p);
}

uintptr_t
sys_ignore(struct LFILinuxThread *t, const char *name);

uintptr_t
sys_nosys(struct LFILinuxThread *t, const char *name);

int
sys_arch_prctl(struct LFILinuxThread *t, int code, lfiptr addr);

// What an exit syscall is asking for.
enum LFIExitKind {
    // exit: this thread only.
    LFI_EXIT_THREAD,
    // exit_group: the whole sandbox. Marks the proc terminating so the other
    // threads leave at their next checkpoint.
    LFI_EXIT_PROCESS,
};

_Noreturn void
thread_exit(struct LFILinuxThread *t, enum LFIExitKind kind, int code);

// The one place a thread that is not itself exiting can be told to exit.
static inline void
sys_checkpoint(struct LFILinuxThread *t)
{
    if (atomic_load_explicit(&t->proc->terminating, memory_order_acquire))
        thread_exit(t, LFI_EXIT_THREAD,
            atomic_load_explicit(&t->proc->exit_code, memory_order_relaxed));
}

uintptr_t
sys_exit_group(struct LFILinuxThread *t, int code);

uintptr_t
sys_brk(struct LFILinuxThread *t, lfiptr addr);

uintptr_t
sys_exit(struct LFILinuxThread *t, int code);

uintptr_t
sys_lfi_pause(struct LFILinuxThread *t);
