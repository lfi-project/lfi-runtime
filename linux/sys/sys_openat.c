#include "path.h"
#include "sys/sys.h"

#include <fcntl.h>
#include <unistd.h>

static int
openflags(int flags)
{
    return ((flags & LINUX_O_RDONLY) ? O_RDONLY : 0) |
        ((flags & LINUX_O_WRONLY) ? O_WRONLY : 0) |
        ((flags & LINUX_O_RDWR) ? O_RDWR : 0) |
        ((flags & LINUX_O_CREAT) ? O_CREAT : 0) |
        ((flags & LINUX_O_APPEND) ? O_APPEND : 0) |
        ((flags & LINUX_O_NONBLOCK) ? O_NONBLOCK : 0) |
        ((flags & LINUX_O_DIRECTORY) ? O_DIRECTORY : 0);
}

int
sys_openat(struct LFILinuxThread *t, int dirfd, lfiptr pathp, int flags,
    int mode)
{
    if (dirfd != LINUX_AT_FDCWD)
        return -LINUX_EBADF;
    char host_path[FILENAME_MAX];
    char *path = pathcopyresolve(t, pathp, host_path, sizeof(host_path));
    if (!path)
        return -LINUX_ENOENT;
    int kfd = open(host_path, openflags(flags), mode);
    if (kfd < 0) {
        LOG(t->proc->engine, "sys_open(\"%s\") = %d", path, host_err(errno));
        free(path);
        return host_err(errno);
    }
    bool isdir = host_checkdir(host_path) == 0;
    // Directories keep their resolved path for fchdir.
    char *dir = isdir ? path : NULL;
    if (!isdir)
        free(path);
    int fd = fdalloc(&t->proc->fdtable, kfd, dir, flags);
    if (fd < 0) {
        close(kfd);
        return fd;
    }
    LOG(t->proc->engine, "sys_open(\"%s\") = %d", host_path, fd);
    return fd;
}
