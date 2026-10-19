#pragma once

// The full Linux configuration. This header is included by linux/hooks.h
// when LINUX_FULL is defined and provides the real hook implementations plus
// the per-proc and per-thread state they maintain.

#include "linux.h"
#include "proc.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Per-proc state of the full configuration, reached via proc->full.
struct LFIFullProc {
    // Total number of threads this proc has created (cumulative). Used to
    // hand out TIDs.
    _Atomic(int) total_thread_count;

    // Number of LFI_THREAD_RUNTIME threads (spawned by sys_clone) still
    // running. lfi_proc_free waits for this to reach zero before freeing
    // anything.
    int active_threads;
    // Guards active_threads.
    pthread_mutex_t lk_threads;
    // Signalled when a runtime thread exits and active_threads changes.
    pthread_cond_t cond_threads;

    // Sandbox functions used to create and destroy the threads that host
    // threads attach to, if the sandbox exports them (see
    // lfi_linux_init_clone).
    lfiptr thread_create;
    lfiptr thread_destroy;

    // The clone context is the context used for cloning new sandbox threads
    // dynamically. To create a new thread, invoke thread_create via the
    // clone_ctx. The sandbox will execute pthread_create inside thread_create,
    // which will make a clone syscall, which will not create a new host thread
    // but will instead place the newly created context into full_new_ctx. The
    // newly created context can then be retrieved from full_new_ctx by the
    // caller. NULL if the sandbox does not export thread_create, in which case
    // the runtime builds the contexts for host threads itself.
    struct LFIContext *clone_ctx;
    // Serializes use of clone_ctx.
    pthread_mutex_t lk_clone;
};

// Per-thread state of the full configuration, reached via t->full.
struct LFIFullThread {
    // This thread's virtual TID.
    int tid;

    // Clear-child-tid pointer: cleared and woken when the thread exits.
    lfiptr ctidp;

    // Set if the sandbox created this thread for a host thread to attach to
    // (via thread_create), so the sandbox must tear it down with
    // thread_destroy.
    bool sandbox_attached;
};

// Where sys_clone places a context created on the proc's clone_ctx, for the
// caller that invoked thread_create to pick up.
extern _Thread_local struct LFIContext *full_new_ctx;

// Called at the end of lfi_proc_new. Returns false on failure, in which case
// lfi_proc_new fails too.
bool
full_proc_init(struct LFILinuxProc *proc);

// Called from proc_destroy, before the proc's remaining state is freed. Waits
// for every runtime-spawned thread to exit first.
void
full_proc_free(struct LFILinuxProc *proc);

// Called from thread_alloc for every new thread. Returns false on failure, in
// which case the thread is not created.
bool
full_thread_init(struct LFILinuxThread *t);

// Called from lfi_thread_free, before the thread's context is freed.
void
full_thread_free(struct LFILinuxThread *t);

// Called by lfi_proc_free, before anything is freed. Waits for every
// runtime-spawned thread to exit.
void
full_proc_join_threads(struct LFILinuxProc *proc);

// Called by thread_exit for every exiting thread. Clears and wakes the
// thread's clear-child-tid address. For an LFI_THREAD_HOST thread, it then
// returns from the sandbox invocation the thread is in, and does not return.
void
full_thread_exit(struct LFILinuxThread *t);

// Called by lfi_linux_init_clone. Sets up the clone context if the sandbox
// exports thread_create and thread_destroy.
void
full_init_clone(struct LFILinuxThread *main_thread);

// Called by the clone callback to create a context for a host thread that is
// entering the sandbox. Returns false if the full configuration does not
// create it (the sandbox does not export thread_create), in which case the
// runtime builds it. Otherwise stores the new thread in *o_t, or NULL on
// failure.
bool
full_clone_thread(struct LFILinuxProc *proc, struct LFILinuxThread **o_t);

// Called before an attached host thread's sandbox thread is freed. Runs the
// sandbox's thread_destroy on it if the sandbox created it.
void
full_detach_thread(struct LFILinuxThread *t);

// Creates a new thread with the registers of the given thread. The caller
// must set up its stack.
struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t, enum LFIThreadOwner owner);

// Called by syshandle before the minimal dispatcher runs. Returns true if the
// syscall was handled, with its result in *o_ret; false lets the minimal
// dispatcher handle (or reject) the syscall.
bool
full_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret);
