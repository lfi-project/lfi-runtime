#define _GNU_SOURCE

#include "host.h"

#include "linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(HAVE_GETRANDOM)
#include <sys/random.h>
#endif

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
    case ENXIO:
        return -LINUX_ENXIO;
    default:
        return -LINUX_EINVAL;
    }
}

#if defined(HAVE_SYS_GETRANDOM) || defined(HAVE_GETRANDOM)
// Kernel ABI values, also provided by <sys/random.h> when it exists.
#ifndef GRND_NONBLOCK
#define GRND_NONBLOCK 0x0001
#endif
#ifndef GRND_RANDOM
#define GRND_RANDOM 0x0002
#endif

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
#if defined(HAVE_SYS_GETRANDOM)
    ssize_t r = syscall(SYS_getrandom, buf, size, randomflags(flags));
    if (r < 0)
        return host_err(errno);
    return r;
#elif defined(HAVE_GETRANDOM)
    ssize_t r = getrandom(buf, size, randomflags(flags));
    if (r < 0)
        return host_err(errno);
    return r;
#elif defined(HAVE_GETENTROPY)
    if (size > 256)
        size = 256;
    if (getentropy(buf, size) < 0)
        return host_err(errno);
    return size;
#else
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return host_err(errno);
    ssize_t r = read(fd, buf, size);
    int err = errno;
    close(fd);
    if (r < 0)
        return host_err(err);
    return r;
#endif
}
