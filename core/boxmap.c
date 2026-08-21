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
        munmap(map->regions[i].mapbase, map->regions[i].mapsize);
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

// Attempt to reserve as much virtual address space as possible, starting with
// 'size'. Returns 0 if it is not able to reserve at least 'threshold'.
static size_t
reserve(size_t size, size_t threshold, void **base)
{
    void *p;
    do {
        p = mmap(NULL, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (p == (void *) -1) {
            size /= 2;
        }
        if (size < threshold)
            return 0;
    } while (p == (void *) -1);
    *base = p;
    return size;
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
        extalloc_delete(alloc);
        return false;
    }

    map->regions[map->nregions++] = (struct AddrRegion) {
        .base = (void *) alignbase,
        .size = alignsize,
        .alloc = alloc,
        .mapbase = base,
        .mapsize = size,
    };

    return true;
}

bool
boxmap_reserve(struct BoxMap *map, size_t size)
{
    size_t total = size;
    size_t min = size;
    size_t totalgot = 0;
    // Total amount to aim for, and the amount below which the reservation
    // counts as failed.
    size_t want = size;
    size_t need = size;

    if (size == 0) {
        // Reserve as much as possible.
        total = tb(256);
        size = tb(255);
        min = gb(32);
        want = size;
        need = min;
    }

    for (int i = 0; i < ADDR_REGION_MAX; i++) {
        void *base;
        size_t got = reserve(size, min, &base);
        if (!got)
            break;
        totalgot += got;
        total = total - got;
        size = total;
        if (!addregion(map, base, got)) {
            munmap(base, got);
            return false;
        }
        if (totalgot >= want)
            break;
    }
    return totalgot >= need;
}

static uintptr_t
allocslot(struct BoxMap *map, size_t size)
{
    for (size_t i = 0; i < map->nregions; i++) {
        // A region without enough contiguous free space is skipped in favor
        // of the next one.
        uintptr_t p = extalloc_alloc(map->regions[i].alloc, size);
        if (p != 0) {
            map->regions[i].active++;
            return p;
        }
    }
    return 0;
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
    return allocslot(map, size);
}

void
boxmap_rmspace(struct BoxMap *map, uintptr_t space, size_t size)
{
    deleteslot(map, space, size);
}
