#pragma once

// The full Linux configuration. This header is included by linux/hooks.h
// when LINUX_FULL is defined and provides the real hook implementations plus
// the per-proc and per-thread state they maintain.

#include "linux.h"
#include "proc.h"

#include <stdbool.h>
#include <stdint.h>

// Per-proc state of the full configuration, reached via proc->full.
struct LFIFullProc {
    // Placeholder so the struct is non-empty. Replace with real state (file
    // descriptor table, working directory, futexes, ...) as it is added.
    int unused;
};

// Per-thread state of the full configuration, reached via t->full.
struct LFIFullThread {
    // Placeholder so the struct is non-empty. Replace with real state (tid,
    // clear-child-tid pointer, ...) as it is added.
    int unused;
};

// Called at the end of lfi_proc_new. Returns false on failure, in which case
// lfi_proc_new fails too.
bool
full_proc_init(struct LFILinuxProc *proc);

// Called from proc_destroy, before the proc's remaining state is freed.
void
full_proc_free(struct LFILinuxProc *proc);

// Called from thread_alloc for every new thread. Returns false on failure, in
// which case the thread is not created.
bool
full_thread_init(struct LFILinuxThread *t);

// Called from lfi_thread_free, before the thread's context is freed.
void
full_thread_free(struct LFILinuxThread *t);

// Called by syshandle before the minimal dispatcher runs. Returns true if the
// syscall was handled, with its result in *o_ret; false lets the minimal
// dispatcher handle (or reject) the syscall.
bool
full_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret);
