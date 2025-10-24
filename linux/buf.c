#include "buf.h"

#include "linux.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

struct Buf
buf_read_file(struct LFILinuxEngine *engine, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
        return (struct Buf) { 0 };

    ssize_t size = lseek(fd, 0, SEEK_END);
    if (size < 0)
        goto err;

    lseek(fd, 0, SEEK_SET);
    void *p = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == (void *) -1)
        goto err;

    return (struct Buf) {
        .fd = fd,
        .data = (uint8_t *) p,
        .size = size,
    };
err:
    close(fd);
    return (struct Buf) { 0 };
}

void
buf_close(struct Buf *buf)
{
    if (buf->fd != -1) {
        close(buf->fd);
        munmap((void *) buf->data, buf->size);
    }
}
