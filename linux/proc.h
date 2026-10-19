#pragma once

#include "linux.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Maximum number of bytes that can be allocated via sys_brk.
#define BRKMAXSIZE (512UL * 1024 * 1024)

// State owned by the full configuration (linux/full). Only allocated when the
// runtime is built with -Dlinux_full=true; see hooks.h.
struct LFIFullProc;
struct LFIFullThread;

// Information from loading the ELF image.
struct ELFLoadInfo {
    lfiptr lastva;
    lfiptr elfentry;
    lfiptr elfbase;
    uint64_t elfphoff;
    uint16_t elfphnum;
    uint16_t elfphentsize;
};

// Used for tracking sections loaded from the ELF image (dynsym/dynstr).
struct ElfSection {
    uint8_t *data;
    size_t size;
};

// Symbols in the sandbox that are used for library sandboxing.
struct LibSymbols {
    lfiptr malloc;
    lfiptr realloc;
    lfiptr calloc;
    lfiptr free;
    lfiptr setjmp;
};

struct LFILinuxProc {
    // Underlying sandbox information.
    struct LFIBox *box;
    struct LFIBoxInfo box_info;

    // Information for managing sys_brk.
    lfiptr brkbase;
    size_t brksize;
    pthread_mutex_t lk_brk;

    // ELF entrypoint for running the process.
    lfiptr entry;
    // ELF load info.
    struct ELFLoadInfo elfinfo;

    // Set when the sandbox asks to exit. Read via lfi_proc_exited.
    _Atomic(bool) terminating;
    // Exit code the sandbox asked for. Only meaningful once terminating is
    // set.
    _Atomic(int) exit_code;

    // After the ELF image is loaded, these ELF sections are tracked (if they
    // exist) to look up dynamic symbols for library calls.
    struct ElfSection dynsym;
    struct ElfSection dynstr;

    struct LibSymbols libsyms;

    // File path for the ELF program loaded for this proc (or NULL).
    char *prog_path;

    // Number of host threads currently lazily attached to this proc via the
    // process-wide pthread_key in trampoline.c. Incremented when a host
    // thread first enters the sandbox via the clone callback, decremented when
    // it exits or is explicitly detached.
    _Atomic(int) attached_threads;
    // Set by lfi_proc_free if it returns without actually freeing the proc
    // because attached_threads is still non-zero. The last detach observes
    // this flag (under lk_proc) and finishes the free.
    bool pending_free;

    // Generic lock that guards everything not covered by a more fine-grained
    // lock.
    pthread_mutex_t lk_proc;

    // Full-configuration state, or NULL in the minimal runtime.
    struct LFIFullProc *full;

    struct LFILinuxEngine *engine;
};

// Who owns the host thread a sandbox thread runs on. This decides how the
// thread leaves the sandbox when it exits, and who reclaims it afterwards.
enum LFIThreadOwner {
    // Created by lfi_thread_new and entered by whoever calls lfi_thread_run.
    LFI_THREAD_MAIN,
    // The runtime created this thread's pthread in sys_clone, so it owns both
    // the host thread and this structure and frees them once the thread leaves
    // the sandbox. Only the full configuration creates these.
    LFI_THREAD_RUNTIME,
    // The context is entered by a host thread calling in through the
    // trampoline, so the embedder owns that thread and the runtime has no
    // handle for it.
    LFI_THREAD_HOST,
};

struct LFILinuxThread {
    // Underlying sandbox context.
    struct LFIContext *ctx;

    // Base of the sandbox stack the runtime mapped for this thread, or 0 if
    // the sandbox provided the stack (threads spawned by sys_clone).
    lfiptr stack;
    size_t stack_size;

    // Who owns the host thread this runs on. See enum LFIThreadOwner.
    enum LFIThreadOwner owner;

    struct LFILinuxProc *proc;

    // Set when this thread exits and unwinds out of lfi_ctx_run.
    _Atomic(bool) exited;

    // Set once the sandbox has handed this context back with the LFI pause
    // syscall, which happens at most once, at the end of its startup.
    bool paused;

    // Full-configuration state, or NULL in the minimal runtime.
    struct LFIFullThread *full;
};

#ifdef __cplusplus
extern "C" {
#endif

// Allocates a thread. Callers still have to set up ctx and any stack.
struct LFILinuxThread *
thread_alloc(struct LFILinuxProc *proc, enum LFIThreadOwner owner);

// Frees a thread from thread_alloc. Does not free its ctx or stack.
void
thread_dealloc(struct LFILinuxThread *t);

// Actually free the proc storage. Used by lfi_proc_free and by the last
// detach when pending_free is set. Callers must ensure no attached thread is
// using the proc anymore (attached_threads == 0). Runtime-spawned threads are
// waited for by the full configuration's full_proc_free.
void
proc_destroy(struct LFILinuxProc *proc);

#ifdef __cplusplus
}
#endif
