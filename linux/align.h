#pragma once

#include <stddef.h>
#include <stdint.h>

#define MIN(a, b)               \
    __extension__({             \
        int _a = (a), _b = (b); \
        _a < _b ? _a : _b;      \
    })

static inline uintptr_t
truncp(uintptr_t addr, size_t align)
{
    size_t align_mask = align - 1;
    return addr & ~align_mask;
}

static inline uintptr_t
ceilp(uintptr_t addr, size_t align)
{
    size_t align_mask = align - 1;
    return (addr + align_mask) & ~align_mask;
}
