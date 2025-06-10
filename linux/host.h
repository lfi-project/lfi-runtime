#pragma once

#include "file.h"

int
host_fstatat(int fd, const char *path, struct Stat *stat_, int flags);

static inline int
host_fstat(int fd, struct Stat *stat_)
{
    return host_fstatat(fd, "", stat_, LINUX_AT_EMPTY_PATH);
}

ssize_t
host_getdents64(int fd, void *dirp, size_t count);
