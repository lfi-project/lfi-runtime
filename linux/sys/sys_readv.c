#include "sys/sys.h"

#define IOV_MAX_LFI 1024

struct IOVec {
    lfiptr base;
    size_t len;
};

ssize_t
sys_readv(struct LFILinuxThread *t, int fd, lfiptr iovp, size_t iovcnt)
{
    if (iovcnt > IOV_MAX_LFI)
        return -LINUX_EINVAL;
    if (iovcnt == 0)
        return 0;
    if (!bufcheck(t, iovp, iovcnt * sizeof(struct IOVec),
            alignof(struct IOVec)))
        return -LINUX_EINVAL;
    struct IOVec *iov = copyout(t, iovp, iovcnt * sizeof(struct IOVec));
    if (!iov)
        return -LINUX_ENOMEM;
    ssize_t total = 0;

    for (size_t i = 0; i < iovcnt; i++) {
        ssize_t n = sys_read(t, fd, iov[i].base, iov[i].len);
        if (n < 0) {
            total = n;
            goto end;
        }
        total += n;
    }

end:
    free(iov);
    return total;
}
