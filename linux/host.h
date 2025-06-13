#pragma once

#include "linux.h"

#include <errno.h>
#include <stdio.h>

#define HOST_ERR(type, expr)                               \
    ({                                                     \
        type _ret = (type) expr;                           \
        _ret == (type) -1 ? (type) host_err(errno) : _ret; \
    })

int
host_fstatat(int fd, const char *path, struct Stat *stat_, int flags);

static inline int
host_fstat(int fd, struct Stat *stat_)
{
    return host_fstatat(fd, "", stat_, LINUX_AT_EMPTY_PATH);
}

ssize_t
host_getdents64(int fd, void *dirp, size_t count);

int
host_err(int err);

ssize_t
host_getrandom(void *buf, size_t size, unsigned int flags);

int
host_isdir(const char *path);
