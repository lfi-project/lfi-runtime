#pragma once

#include "futex.h"
#include "linux.h"
#include "list.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Maximum number of file descriptors the LFI runtime process can have open.
#define LINUX_NOFILE 1024
// Maximum number of bytes that can be allocated via sys_brk.
#define BRKMAXSIZE (512UL * 1024 * 1024)

struct FDTable {
    // File descriptor conversion table.
    int fds[LINUX_NOFILE];
    // Full sandbox path for opened directories. This is necessary for
    // supporting fchdir(fd).
    char *dirs[LINUX_NOFILE];
    // Open flags for each file descriptor.
    int flags[LINUX_NOFILE];
    pthread_mutex_t lk;

    bool passthrough;
};

// Information from loading the ELF image.
struct ELFLoadInfo {
    lfiptr lastva;
    lfiptr elfentry;
    lfiptr ldentry;
    lfiptr elfbase;
    lfiptr ldbase;
    uint64_t elfphoff;
    uint16_t elfphnum;
    uint16_t elfphentsize;
};

// Used for tracking the current working directory.
struct Dir {
    char path[FILENAME_MAX];
    pthread_mutex_t lk;
};

// Used for tracking sections loaded from the ELF image (dynsym/dynstr).
struct ElfSection {
    uint8_t *data;
    size_t size;
};

// Symbols in the sandbox that are used for library sandboxing.
struct LibSymbols {
    lfiptr thread_create;
    lfiptr thread_destroy;
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

    // File descriptor table.
    struct FDTable fdtable;

    // Current working directory.
    struct Dir cwd;

    // Futex backend state for this process. May be null if a native API is
    // used (Linux/macOS).
    struct Futexes *futexes;

    // Set when the sandbox asks to exit. Read via lfi_proc_exited.
    _Atomic(bool) terminating;
    // Exit code the sandbox asked for. Only meaningful once terminating is
    // set.
    _Atomic(int) exit_code;

    // Total number of threads this proc has spawned (cumulative).
    _Atomic(int) total_thread_count;
    // Threads this proc is responsible for freeing when it is destroyed: the
    // LFI_THREAD_RUNTIME threads sys_clone spawned. LFI_THREAD_HOST
    // attachments are not tracked here. They are owned by the host thread that
    // attached and are freed by the pthread-key destructor in trampoline.c.
    // sys_minimal has no sys_clone, so this list stays empty there.
    struct List *threads;
    // Number of runtime-spawned threads still running. lfi_proc_free waits for
    // this to reach zero before freeing anything.
    int active_threads;
    // Guards threads and active_threads.
    pthread_mutex_t lk_threads;
    // Signalled when a thread exits and active_threads changes.
    pthread_cond_t cond_threads;

    // After the ELF image is loaded, these ELF sections are tracked (if they
    // exist) to look up dynamic symbols for library calls.
    struct ElfSection dynsym;
    struct ElfSection dynstr;

    struct LibSymbols libsyms;

    // File path for ELF interpreter used to load this proc (or NULL).
    char *interp_path;
    // File path for the ELF program loaded for this proc (or NULL).
    char *prog_path;

    // The clone context is the context used for cloning new sandbox threads
    // dynamically. To create a new thread, invoke thread_create via the
    // clone_ctx. The sandbox will execute pthread_create inside thread_create,
    // which will make a clone syscall, which will not create a new thread but
    // will instead place the newly created context into new_ctx. The newly
    // created context can then be retrieved from new_ctx by the caller.
    struct LFIContext *clone_ctx;
    pthread_mutex_t lk_clone;

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

    struct LFILinuxEngine *engine;
};

// Who owns the host thread a sandbox thread runs on. This decides how the
// thread leaves the sandbox when it exits, and who reclaims it afterwards.
enum LFIThreadOwner {
    // Created by lfi_thread_new and entered by whoever calls lfi_thread_run.
    LFI_THREAD_MAIN,
    // The runtime created this thread's pthread in sys_clone, so it owns both
    // the host thread and this structure and frees them once the thread leaves
    // the sandbox.
    LFI_THREAD_RUNTIME,
    // The context is entered by a host thread calling in through the
    // trampoline, so the embedder owns that thread and the runtime has no
    // handle for it.
    LFI_THREAD_HOST,
};

struct LFILinuxThread {
    // Underlying sandbox context.
    struct LFIContext *ctx;

    // Pointer to base of sandbox stack (this will be NULL for threads that
    // were spawned by the sandbox).
    lfiptr stack;
    size_t stack_size;

    // Child tid pointer location.
    lfiptr ctidp;

    // This thread's virtual TID.
    int tid;

    // Pthread object. Only set, and only valid, for LFI_THREAD_RUNTIME.
    pthread_t *pthread;

    // Who owns the host thread this runs on. See enum LFIThreadOwner.
    enum LFIThreadOwner owner;

    // Element in the parent proc's threads list.
    struct List threads_elem;

    struct LFILinuxProc *proc;

    // Set when this thread exits and unwinds out of lfi_ctx_run.
    _Atomic(bool) exited;

    // Set once the sandbox has handed this context back with the LFI pause
    // syscall, which happens at most once, at the end of its startup.
    bool paused;
};

#ifdef __cplusplus
extern "C" {
#endif

int
proc_mapany(struct LFILinuxProc *p, size_t size, int prot, int flags, int fd,
    off_t offset, lfiptr *o_mapstart);

int
proc_mapat(struct LFILinuxProc *p, lfiptr start, size_t size, int prot,
    int flags, int fd, off_t offset);

int
proc_unmap(struct LFILinuxProc *p, lfiptr start, size_t size);

// Allocates a thread. Callers still have to set up ctx and any stack.
struct LFILinuxThread *
thread_alloc(struct LFILinuxProc *proc, enum LFIThreadOwner owner);

struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t, enum LFIThreadOwner owner);

int
proc_chdir(struct LFILinuxProc *p, const char *path);

// Actually free the proc storage. Used by lfi_proc_free and by the last
// detach when pending_free is set. Callers must ensure no thread is using the
// proc anymore (active_threads == 0, attached_threads == 0).
void
proc_destroy(struct LFILinuxProc *proc);

#ifdef __cplusplus
}
#endif
