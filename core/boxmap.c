#include "boxmap.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static uintptr_t
truncp(uintptr_t addr, size_t align)
{
    return addr - (addr % align);
}

static uintptr_t
ceilp(uintptr_t addr, size_t align)
{
    uintptr_t rem = addr % align;
    if (rem == 0) {
        return addr;
    }
    return addr + (align - rem);
}

static size_t
gb(size_t x)
{
    return x * 1024 * 1024 * 1024;
}

static size_t
tb(size_t x)
{
    return x * 1024 * 1024 * 1024 * 1024;
}

struct BoxMap *
boxmap_new(struct BoxMapOptions opts)
{
    struct BoxMap *map = calloc(sizeof(struct BoxMap), 1);
    if (!map)
        return NULL;
    map->opts = opts;
    return map;
}

void
boxmap_delete(struct BoxMap *map)
{
    for (size_t i = 0; i < map->nregions; i++) {
        munmap(map->regions[i].base, map->regions[i].size);
        extalloc_delete(map->regions[i].alloc);
    }

    free(map);
}

uint64_t
boxmap_size(struct BoxMap *map)
{
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].size;
    }
    return total;
}

uint64_t
boxmap_active(struct BoxMap *map)
{
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].active;
    }
    return total;
}

enum {
    RESERVE_FORBID_BIT = 47,
    // Maximum number of hinted retries at a given size before shrinking the
    // request.
    RESERVE_MAX_RETRY = 64,
};

// Returns true if any address in the half-open region [base, base+size) has bit
// RESERVE_FORBID_BIT set.
static bool
region_forbidden(uintptr_t base, size_t size)
{
    if (size == 0)
        return false;
    uintptr_t last = base + size - 1;
    if ((base >> RESERVE_FORBID_BIT) != (last >> RESERVE_FORBID_BIT))
        return true;
    return (base & ((uintptr_t) 1 << RESERVE_FORBID_BIT)) != 0;
}

// Attempt to reserve as much virtual address space as possible, starting with
// 'size'. Returns 0 if it is not able to reserve at least 'threshold'.
//
// Never returns a region that contains an address with bit RESERVE_FORBID_BIT
// set. The OS chooses the placement first; if that region includes a forbidden
// address it is released and the reservation is retried with a hint that places
// it below the boundary, lowering the hint until the OS honors a clean spot.
static size_t
reserve(size_t size, size_t threshold, void **base)
{
    uintptr_t top = (uintptr_t) 1 << RESERVE_FORBID_BIT;

    while (size >= threshold) {
        // hint == 0 (NULL) lets the OS choose the placement.
        uintptr_t hint = 0;
        for (size_t tries = 0; tries < RESERVE_MAX_RETRY; tries++) {
            void *p = mmap((void *) hint, size, PROT_NONE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (p == (void *) -1)
                break;
            if (!region_forbidden((uintptr_t) p, size)) {
                *base = p;
                return size;
            }
            // Release the forbidden region and retry below the boundary.
            munmap(p, size);
            if (size >= top)
                break;
            hint = hint == 0 ? top - size : hint / 2;
            if (hint == 0)
                break;
        }
        size /= 2;
    }
    return 0;
}

static bool
addregion(struct BoxMap *map, void *base, size_t size)
{
    if (map->nregions >= ADDR_REGION_MAX) {
        return false;
    }

    // Find a chunk-aligned sub-region that has at least guardsize bytes of
    // space on each side. The guard regions are the space between the mmap
    // edges and the chunk-aligned allocatable region.
    uintptr_t start = (uintptr_t) base + map->opts.guardsize;
    uintptr_t end = (uintptr_t) base + size - map->opts.guardsize;

    uintptr_t alignbase = ceilp(start, map->opts.chunksize);
    size_t alignsize = truncp(end, map->opts.chunksize) - alignbase;

    struct ExtAlloc *alloc = extalloc_new(alignbase, alignsize,
        map->opts.chunksize);
    if (!alloc)
        return false;

    void *region = mmap((void *) alignbase, alignsize, PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (region != (void *) alignbase) {
        free(alloc);
        return false;
    }

    map->regions[map->nregions++] = (struct AddrRegion) {
        .base = (void *) alignbase,
        .size = alignsize,
        .alloc = alloc,
    };

    return true;
}

bool
boxmap_reserve(struct BoxMap *map, size_t size)
{
    size_t total = size;
    size_t min = size;
    size_t totalgot = 0;

    if (size == 0) {
        total = tb(256);
        size = tb(255);
        min = gb(32);
    }
    size_t i_size = size;

    int i;
    for (i = 0; i < ADDR_REGION_MAX; i++) {
        void *base;
        size_t got = reserve(size, min, &base);
        if (!got)
            break;
        totalgot += got;
        total = total - got;
        size = total;
        if (!addregion(map, base, got))
            return false;
        if (totalgot >= i_size)
            break;
    }
    if (totalgot < i_size) {
        return false;
    }
    return true;
}

static bool
isfull(struct BoxMap *map)
{
    for (size_t i = 0; i < map->nregions; i++) {
        if (!extalloc_is_full(map->regions[i].alloc))
            return false;
    }
    return true;
}

// This function can only be called if the engine is not full.
static uintptr_t
allocslot(struct BoxMap *map, size_t size)
{
    for (size_t i = 0; i < map->nregions; i++) {
        if (!extalloc_is_full(map->regions[i].alloc)) {
            map->regions[i].active++;
            return extalloc_alloc(map->regions[i].alloc, size);
        }
    }
    __builtin_unreachable();
}

static void
deleteslot(struct BoxMap *map, uintptr_t base, size_t size)
{
    for (size_t i = 0; i < map->nregions; i++) {
        uintptr_t vabase = (uintptr_t) map->regions[i].base;
        if (base >= vabase && base < vabase + map->regions[i].size) {
            extalloc_free(map->regions[i].alloc, base, size);
            map->regions[i].active--;
        }
    }
}

uintptr_t
boxmap_addspace(struct BoxMap *map, size_t size)
{
    if (isfull(map)) {
        return 0;
    }

    return allocslot(map, size);
}

void
boxmap_rmspace(struct BoxMap *map, uintptr_t space, size_t size)
{
    deleteslot(map, space, size);
}
