#include "sys/sys.h"

struct IOVec {
    lfiptr base;
    size_t len;
};

ssize_t
sys_writev(struct LFILinuxThread *t, int fd, lfiptr iovp, size_t iovcnt)
{
    if (!bufcheck(t, iovp, iovcnt * sizeof(struct IOVec),
            alignof(struct IOVec)))
        return -LINUX_EINVAL;
    struct IOVec *iov = copyout(t, iovp, iovcnt * sizeof(struct IOVec));
    if (!iov)
        return -LINUX_ENOMEM;
    ssize_t total = 0;

    for (size_t i = 0; i < iovcnt; i++) {
        ssize_t n = sys_write(t, fd, iov[i].base, iov[i].len);
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
