#pragma once

#include "futex.h"
#include "linux.h"

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
    const char *dirs[LINUX_NOFILE];
    pthread_mutex_t lk;

    bool passthrough;
};

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

struct Dir {
    char path[FILENAME_MAX];
    pthread_mutex_t lk;
};

struct LFILinuxProc {
    // Underlying sandbox information.
    struct LFIBox *box;
    struct LFIBoxInfo box_info;
    pthread_mutex_t lk_box;

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

    // Futexes for this process.
    struct Futexes futexes;

    // Total number of threads this proc has spawned (cumulative).
    _Atomic(int) total_thread_count;
    // List of threads currently spawned in this process via a clone that
    // originated in the sandbox (not via a host thread).
    struct List *threads;
    // Lock for threads.
    int active_threads;
    // Lock for thread variables above.
    pthread_mutex_t lk_threads;
    // Conditional variable to signal when thread exits.
    pthread_cond_t cond_threads;

    struct LFILinuxEngine *engine;
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

    // Pthread object if this thread was spawned by the LFI runtime using a
    // pthread.
    pthread_t *pthread;

    // Element in the parent proc's threads list.
    struct List threads_elem;

    // Used to signal the condition variable that the thread is ready.
    bool ready;
    pthread_mutex_t lk_ready;
    pthread_cond_t cond_ready;

    struct LFILinuxProc *proc;
};

int
proc_mapany(struct LFILinuxProc *p, size_t size, int prot, int flags, int fd,
    off_t offset, lfiptr *o_mapstart);

int
proc_mapat(struct LFILinuxProc *p, lfiptr start, size_t size, int prot,
    int flags, int fd, off_t offset);

int
proc_unmap(struct LFILinuxProc *p, lfiptr start, size_t size);

struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t);

int
proc_chdir(struct LFILinuxProc *p, const char *path);
