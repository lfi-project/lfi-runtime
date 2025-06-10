#pragma once

#include "futex.h"
#include "linux.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Maximum number of file descriptors a process can have open.
#define LINUX_NOFILE 128
// Maximum number of bytes that can be allocated via sys_brk.
#define BRKMAXSIZE (512UL * 1024 * 1024)

struct FDFile {
    // Underlying device pointer, passed as the first argument to all file
    // operations.
    void *dev;

    // File refcount.
    size_t refs;
    pthread_mutex_t lk_refs;

    // Virtual function table for file operations.
    ssize_t (*read)(void *, uint8_t *, size_t);
    ssize_t (*write)(void *, uint8_t *, size_t);
    ssize_t (*lseek)(void *, off_t, int);
    int (*close)(void *);
    int (*stat_)(void *, struct Stat *);
    ssize_t (*getdents)(void *, void *, size_t);
    int (*chown)(void *, linux_uid_t, linux_gid_t);
    int (*chmod)(void *, linux_mode_t);
    int (*truncate)(void *, off_t);
    int (*sync)(void *);
    int (*filefd)(void *);
};

struct FDTable {
    struct FDFile *files[LINUX_NOFILE];
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
proc_mapat(struct LFILinuxProc* p, lfiptr start, size_t size, int prot, int flags,
        int fd, off_t offset);

int
proc_unmap(struct LFILinuxProc* p, lfiptr start, size_t size);
