#pragma once

#include "boxmap.h"

enum {
    ADDR_REGION_MAX = 16,
};

struct ExtAlloc {
    uintptr_t base;
    size_t size;
    size_t chunksize;

    uint8_t *bitvec;
};

struct AddrRegion {
    void* base;
    size_t size;
    size_t active;
    struct ExtAlloc* alloc;
};

struct BoxMap {
    struct AddrRegion regions[ADDR_REGION_MAX];
    size_t nregions;

    struct BoxMapOptions opts;
};

struct ExtAlloc * extalloc_new(uintptr_t base, size_t size, size_t chunksize);

bool extalloc_is_full(struct ExtAlloc *a);

uintptr_t extalloc_alloc(struct ExtAlloc *a, size_t n);

void extalloc_allocat(struct ExtAlloc *a, uintptr_t at, size_t n);

void extalloc_free(struct ExtAlloc *a, uintptr_t at, size_t n);

void extalloc_delete(struct ExtAlloc *a);
