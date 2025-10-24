#pragma once

#include "linux.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct Buf {
    int fd;
    const uint8_t *data;
    size_t size;
};

static inline size_t
buf_read(struct Buf buf, void *to, size_t count, size_t offset)
{
    if (offset + count > buf.size)
        count = buf.size - offset;
    memcpy(to, &buf.data[offset], count);
    return count;
}

struct Buf
buf_read_file(struct LFILinuxEngine *engine, const char *filename);

void
buf_close(struct Buf *buf);
