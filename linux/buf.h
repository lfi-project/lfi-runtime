#pragma once

#include "linux.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

struct Buf {
    uint8_t *data;
    size_t size;
};

static inline bool
buf_write(struct Buf* buf, uint8_t* data, size_t size)
{
    uint8_t* b = realloc(buf->data, buf->size + size);
    if (!b)
        return false;
    buf->data = b;
    memcpy(&buf->data[buf->size], data, size);
    buf->size += size;
    return true;
}

static inline size_t
buf_read(struct Buf buf, void* to, size_t count, size_t offset)
{
    if (offset + count > buf.size)
        count = buf.size - offset;
    memcpy(to, &buf.data[offset], count);
    return count;
}

struct Buf
buf_read_file(struct LFILinuxEngine *engine, const char *filename);
