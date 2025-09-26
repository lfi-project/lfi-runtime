#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "err.h"
#include "map.h"

static uintptr_t truncp(uintptr_t addr, size_t align) {
    return addr - (addr % align);
}

static uintptr_t ceilp(uintptr_t addr, size_t align) {
    uintptr_t rem = addr % align;
    if (rem == 0) {
        return addr;
    }
    return addr + (align - rem);
}

static size_t gb(size_t x) {
    return x * 1024 * 1024 * 1024;
}

static size_t tb(size_t x) {
    return x * 1024 * 1024 * 1024 * 1024;
}

struct BoxMap* boxmap_new(struct BoxMapOptions opts) {
    struct BoxMap* map = calloc(1, sizeof(struct BoxMap));
    if (!map)
        return NULL;
    map->opts = opts;
    return map;
}

void boxmap_delete(struct BoxMap* map) {
    for (size_t i = 0; i < map->nregions; i++) {
        munmap(map->regions[i].base, map->regions[i].size);
        extalloc_delete(map->regions[i].alloc);
    }

    free(map);
}

uint64_t boxmap_size(struct BoxMap* map) {
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].size - 2 * map->opts.guardsize;
    }
    return total;
}

uint64_t boxmap_active(struct BoxMap* map) {
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].active;
    }
    return total;
}

// Attempt to reserve as much virtual address space as possible, starting with
// 'size'. Returns 0 if it is not able to reserve at least 'threshold'.
static size_t reserve(size_t size, size_t threshold, void** base) {
    void* p;
    do {
        p = mmap(NULL, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (p == (void*) -1) {
            size /= 2;
        }
        if (size < threshold)
            return 0;
    } while (p == (void*) -1);
    *base = p;
    return size;
}

static bool addregion(struct BoxMap* map, void* base, size_t size) {
    if (map->nregions >= ADDR_REGION_MAX) {
        g_err = "boxmap: no available regions";
        return false;
    }

    // Since mmap gives us something page-aligned, we need to find a region
    // within it that is properly chunk-aligned.
    uintptr_t alignbase = ceilp((uintptr_t) base, map->opts.chunksize);
    size_t alignsize = truncp(alignbase + (size - (alignbase - (uintptr_t) base)), map->opts.chunksize) - alignbase;

    struct ExtAlloc* alloc = extalloc_new(alignbase, alignsize, map->opts.chunksize);
    if (!alloc)
        return false;

    // Reserve the guard regions on either end of the new region.
    extalloc_allocat(alloc, alignbase, map->opts.guardsize);
    extalloc_allocat(alloc, alignbase + alignsize - map->opts.guardsize, map->opts.guardsize);

    map->regions[map->nregions++] = (struct AddrRegion) {
        .base = (void*) alignbase,
        .size = alignsize,
        .alloc = alloc,
    };

    return true;
}

bool boxmap_reserve(struct BoxMap* map, size_t size) {
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
        void* base;
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
        g_err = "boxmap: cannot mmap";
        return false;
    }
    return true;
}

static bool isfull(struct BoxMap* map) {
    for (size_t i = 0; i < map->nregions; i++) {
        if (!extalloc_is_full(map->regions[i].alloc))
            return false;
    }
    return true;
}

// This function can only be called if the engine is not full.
static uintptr_t allocslot(struct BoxMap* map, size_t size) {
    for (size_t i = 0; i < map->nregions; i++) {
        if (!extalloc_is_full(map->regions[i].alloc)) {
            map->regions[i].active++;
            return extalloc_alloc(map->regions[i].alloc, size);
        }
    }
    assert(!"unreachable: engine was full");
}

static void deleteslot(struct BoxMap* map, uintptr_t base, size_t size) {
    for (size_t i = 0; i < map->nregions; i++) {
        uintptr_t vabase = (uintptr_t) map->regions[i].base;
        if (base >= vabase && base < vabase + map->regions[i].size) {
            extalloc_free(map->regions[i].alloc, base, size);
            map->regions[i].active--;
        }
    }
}

uintptr_t boxmap_addspace(struct BoxMap* map, size_t size) {
    if (isfull(map)) {
        g_err = "boxmap: no available slots";
        return 0;
    }

    return allocslot(map, size);
}

void boxmap_rmspace(struct BoxMap* map, uintptr_t space, size_t size) {
    deleteslot(map, space, size);
}
