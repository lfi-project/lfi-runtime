#pragma once

#include "linux.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Maximum number of file descriptors a process can have open.
#define LINUX_NOFILE 128

struct FDFile {
    void *dev;
    size_t refs;
    pthread_mutex_t lk_refs;

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
};

struct FDTable {
    struct FDFile *files[LINUX_NOFILE];
    pthread_mutex_t lk;
};
