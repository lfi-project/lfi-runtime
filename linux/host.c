#define _GNU_SOURCE

#include "host.h"

#include "align.h"
#include "linux.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

int
host_err(int err)
{
    switch (err) {
    case ENOSYS:
        return -LINUX_ENOSYS;
    case EINVAL:
        return -LINUX_EINVAL;
    case ENOENT:
        return -LINUX_ENOENT;
    case EBADF:
        return -LINUX_EBADF;
    case EAGAIN:
        return -LINUX_EAGAIN;
    case EPERM:
        return -LINUX_EPERM;
    case ENOMEM:
        return -LINUX_ENOMEM;
    case EACCES:
        return -LINUX_EACCES;
    case ENOTDIR:
        return -LINUX_ENOTDIR;
    case EMFILE:
        return -LINUX_EMFILE;
    case EFAULT:
        return -LINUX_EFAULT;
    default:
        return -LINUX_EINVAL;
    }
}

#if defined(HAVE_GETRANDOM)
#include <sys/random.h>

static unsigned int
randomflags(unsigned int flags)
{
    unsigned int f = 0;
    if ((flags & LINUX_GRND_RANDOM) != 0)
        f |= GRND_RANDOM;
    if ((flags & LINUX_GRND_NONBLOCK) != 0)
        f |= GRND_NONBLOCK;
    return f;
}
#endif

ssize_t
host_getrandom(void *buf, size_t size, unsigned int flags)
{
#if defined(HAVE_GETRANDOM)
    ssize_t r = getrandom(buf, size, randomflags(flags));
    if (r < 0)
        return host_err(errno);
    return r;
#else
    if (size > 256)
        size = 256;
    int r = getentropy(buf, size);
    if (r < 0)
        return host_err(errno);
    return size;
#endif
}

static void
copystat(struct Stat *stat_, struct stat *kstat)
{
    stat_->st_dev = kstat->st_dev;
    stat_->st_ino = kstat->st_ino;
    stat_->st_nlink = kstat->st_nlink;
    stat_->st_mode = kstat->st_mode;
    stat_->st_uid = kstat->st_uid;
    stat_->st_gid = kstat->st_gid;
    stat_->st_rdev = kstat->st_rdev;
    stat_->st_size = kstat->st_size;
    stat_->st_blksize = kstat->st_blksize;
    stat_->st_blocks = kstat->st_blocks;
#ifdef __APPLE__
    stat_->st_atim.sec = kstat->st_atimespec.tv_sec;
    stat_->st_atim.nsec = kstat->st_atimespec.tv_nsec;
    stat_->st_mtim.sec = kstat->st_mtimespec.tv_sec;
    stat_->st_mtim.nsec = kstat->st_mtimespec.tv_nsec;
    stat_->st_ctim.sec = kstat->st_ctimespec.tv_sec;
    stat_->st_ctim.nsec = kstat->st_ctimespec.tv_nsec;
#else
    stat_->st_atim.sec = kstat->st_atim.tv_sec;
    stat_->st_atim.nsec = kstat->st_atim.tv_nsec;
    stat_->st_mtim.sec = kstat->st_mtim.tv_sec;
    stat_->st_mtim.nsec = kstat->st_mtim.tv_nsec;
    stat_->st_ctim.sec = kstat->st_ctim.tv_sec;
    stat_->st_ctim.nsec = kstat->st_ctim.tv_nsec;
#endif
}

int
host_fstatat(int fd, const char *path, struct Stat *stat_, int flags)
{
    struct stat kstat;
    int r;
    if (flags == LINUX_AT_EMPTY_PATH) {
        r = fstat(fd, &kstat);
    } else {
        assert(flags == 0);
        r = fstatat(fd == LINUX_AT_FDCWD ? AT_FDCWD : fd, path, &kstat, 0);
    }
    if (r < 0)
        return host_err(errno);
    copystat(stat_, &kstat);
    return 0;
}

#if defined(HAVE_GETDENTS64) || defined(HAVE_SYS_GETDENTS64)

#include <dirent.h>
#include <limits.h>
#include <sys/syscall.h>

#if !defined(HAVE_GETDENTS64)
static int
getdents64(int fd, struct dirent *buf, size_t len)
{
    if (len > INT_MAX)
        len = INT_MAX;
    return HOST_ERR(int, syscall(SYS_getdents64, fd, buf, len));
}
#endif

ssize_t
host_getdents64(int fd, void *dirp, size_t count)
{
    return HOST_ERR(ssize_t, getdents64(fd, dirp, count));
}

#else

#include <dirent.h>
#include <string.h>

ssize_t
host_getdents64(int fd, void *dirp, size_t count)
{
    DIR *dir = fdopendir(fd);
    if (!dir)
        return host_err(errno);
    size_t written = 0;
    while (written < count) {
        struct dirent *ent = readdir(dir);
        if (!ent)
            break;
        struct Dirent *dent = (struct Dirent *) &dirp[written];
        memset(dent, 0, sizeof(struct Dirent));
        dent->d_ino = ent->d_ino;
        dent->d_off = -1;
        // TODO: convert d_type properly
        size_t len = strlen(ent->d_name);
        len = len >= sizeof(dent->d_name) ? sizeof(dent->d_name) - 1 : len;
        dent->d_type = ent->d_type;
        dent->d_reclen = ceilp(8 + 8 + 2 + 1 + len + 1, 8);
        strncpy(dent->d_name, ent->d_name, len);
        written += dent->d_reclen;
    }
    if (errno < 0)
        return host_err(errno);
    return written;
}

#endif

int
host_isdir(const char *path)
{
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return -LINUX_ENOENT;
    }
    if (!S_ISDIR(path_stat.st_mode))
        return -LINUX_ENOTDIR;
    return 0;
}
