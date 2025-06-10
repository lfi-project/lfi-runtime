#include "file.h"

#include "host.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

struct FDFile *
filenew(struct LFILinuxEngine *engine, int host_dir, const char *path,
    int flags, int mode)
{
    int fd = openat(host_dir, path, flags, mode);
    if (fd < 0)
        return NULL;
    return filefdnew(fd, flags);
}

static int
filefd(void *dev)
{
    return ((struct File *) dev)->host_fd;
}

static ssize_t
fileread(void *dev, uint8_t *buf, size_t buflen)
{
    return read(filefd(dev), buf, buflen);
}

static ssize_t
filewrite(void *dev, uint8_t *buf, size_t buflen)
{
    return write(filefd(dev), buf, buflen);
}

static int
linux_whence(int whence)
{
    switch (whence) {
    case LINUX_SEEK_SET:
        return SEEK_SET;
    case LINUX_SEEK_CUR:
        return SEEK_CUR;
    case LINUX_SEEK_END:
        return SEEK_END;
    }
    return -1;
}

static ssize_t
filelseek(void *dev, off_t off, int whence)
{
    int host_whence = linux_whence(whence);
    if (host_whence == -1)
        return -LINUX_EINVAL;
    return lseek(filefd(dev), off, host_whence);
}

static int
filetruncate(void *dev, off_t length)
{
    return ftruncate(filefd(dev), length);
}

static int
filechown(void *dev, linux_uid_t owner, linux_gid_t group)
{
    return fchown(filefd(dev), owner, group);
}

static int
filechmod(void *dev, linux_mode_t mode)
{
    return fchmod(filefd(dev), mode);
}

static int
fileclose(void *dev)
{
    int x = close(filefd(dev));
    return x;
}

static int
filestat(void *dev, struct Stat *stat)
{
    return host_fstat(filefd(dev), stat);
}

static int
filesync(void *dev)
{
    return fsync(filefd(dev));
}

static ssize_t
filegetdents(void *dev, void *dirp, size_t count)
{
    return host_getdents64(filefd(dev), (struct Dirent *) dirp, count);
}

struct FDFile *
filefdnew(int kfd, int flags)
{
    struct File *f = malloc(sizeof(struct File));
    if (!f)
        goto err1;
    *f = (struct File) {
        .host_fd = kfd,
        .flags = flags,
    };
    struct FDFile *ff = malloc(sizeof(struct FDFile));
    if (!ff)
        goto err2;
    *ff = (struct FDFile) {
        .dev = f,
        .refs = 0,
        .read = fileread,
        .write = filewrite,
        .lseek = filelseek,
        .close = fileclose,
        .stat_ = filestat,
        .truncate = filetruncate,
        .chown = filechown,
        .chmod = filechmod,
        .sync = filesync,
        .getdents = filegetdents,
        .filefd = filefd,
    };
    pthread_mutex_init(&ff->lk_refs, NULL);
    return ff;
err2:
    free(f);
err1:
    return NULL;
}
