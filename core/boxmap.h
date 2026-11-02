#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct BoxMapOptions {
    // Size in bytes of the minimum allocatable chunk. Spaces are aligned to it.
    size_t chunksize;
    // Size in bytes to keep reserved below and above the allocatable part of
    // each region. Multiples of the page size.
    size_t guard_lo;
    size_t guard_hi;
};

struct BoxMap;

// boxmap_new creates a new BoxMap structure with no reserved regions.
struct BoxMap *
boxmap_new(struct BoxMapOptions opts);

// boxmap_delete unmaps all regions and frees a BoxMap structure.
void
boxmap_delete(struct BoxMap *map);

// boxmap_size returns the total size available for allocation in 'map'.
uint64_t
boxmap_size(struct BoxMap *map);

// boxmap_active returns the number of active spaces in 'map'.
uint64_t
boxmap_active(struct BoxMap *map);

// boxmap_reserve reserves a region with 'size' bytes of allocatable space (a
// multiple of the chunk size), plus the guards. The region stays reserved
// until the map is deleted. Returns false if an error occurred.
bool
boxmap_reserve(struct BoxMap *map, size_t size);

// boxmap_addspace allocates a space of size 'size' (a multiple of the chunk
// size) in 'map'. If no reserved region has room, it reserves a new region for
// just this space, which is unmapped again when the space is removed. Returns 0
// if an error occurred.
uintptr_t
boxmap_addspace(struct BoxMap *map, size_t size);

// boxmap_rmspace removes the given space from 'map'.
void
boxmap_rmspace(struct BoxMap *map, uintptr_t space, size_t size);

struct ExtAlloc {
    uintptr_t base;
    size_t size;
    size_t chunksize;

    uint8_t *bitvec;
};

struct AddrRegion {
    // Allocatable part of the region.
    void *base;
    size_t size;
    size_t active;
    struct ExtAlloc *alloc;

    // The span that is mapped for this region: the allocatable part plus the
    // guards.
    void *mapbase;
    size_t mapsize;

    // Set for regions that boxmap_addspace reserved on demand. These are
    // unmapped when their last space is removed.
    bool ondemand;
};

struct BoxMap {
    struct AddrRegion *regions;
    size_t nregions;
    size_t capregions;

    struct BoxMapOptions opts;

    // Protects the regions and their allocators.
    pthread_mutex_t lk;
};

struct ExtAlloc *
extalloc_new(uintptr_t base, size_t size, size_t chunksize);

bool
extalloc_is_full(struct ExtAlloc *a);

uintptr_t
extalloc_alloc(struct ExtAlloc *a, size_t n);

void
extalloc_allocat(struct ExtAlloc *a, uintptr_t at, size_t n);

void
extalloc_free(struct ExtAlloc *a, uintptr_t at, size_t n);

void
extalloc_delete(struct ExtAlloc *a);
