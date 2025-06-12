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
    int fds[LINUX_NOFILE];
    pthread_mutex_t lk;
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
    int fd;
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

    // Total number of threads this proc has spawned.
    _Atomic(int) threads;

    struct LFILinuxEngine *engine;
};

struct LFILinuxThread {
    // Underlying sandbox context.
    struct LFIContext *ctx;

    // Pointer to base of sandbox stack.
    lfiptr stack;
    size_t stack_size;

    // Child tid pointer location.
    lfiptr ctidp;

    // This thread's virtual TID.
    int tid;

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
