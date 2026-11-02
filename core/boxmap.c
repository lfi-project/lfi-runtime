#include "boxmap.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static uintptr_t
ceilp(uintptr_t addr, size_t align)
{
    uintptr_t rem = addr % align;
    if (rem == 0) {
        return addr;
    }
    return addr + (align - rem);
}

struct BoxMap *
boxmap_new(struct BoxMapOptions opts)
{
    struct BoxMap *map = calloc(sizeof(struct BoxMap), 1);
    if (!map)
        return NULL;
    if (pthread_mutex_init(&map->lk, NULL) != 0) {
        free(map);
        return NULL;
    }
    map->opts = opts;
    return map;
}

static void
unmapregion(struct AddrRegion *r)
{
    munmap(r->mapbase, r->mapsize);
    extalloc_delete(r->alloc);
}

void
boxmap_delete(struct BoxMap *map)
{
    for (size_t i = 0; i < map->nregions; i++)
        unmapregion(&map->regions[i]);
    free(map->regions);
    pthread_mutex_destroy(&map->lk);
    free(map);
}

uint64_t
boxmap_size(struct BoxMap *map)
{
    pthread_mutex_lock(&map->lk);
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].size;
    }
    pthread_mutex_unlock(&map->lk);
    return total;
}

uint64_t
boxmap_active(struct BoxMap *map)
{
    pthread_mutex_lock(&map->lk);
    size_t total = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        total += map->regions[i].active;
    }
    pthread_mutex_unlock(&map->lk);
    return total;
}

// Reserves a region with 'size' bytes of chunk-aligned allocatable space, with
// guard_lo bytes reserved below it and guard_hi bytes above it, and appends it
// to the map. Returns a pointer to the new region, or NULL on failure. Must be
// called with the map locked.
static struct AddrRegion *
addregion(struct BoxMap *map, size_t size, bool ondemand)
{
    size_t chunk = map->opts.chunksize;
    size_t lo = map->opts.guard_lo;
    size_t hi = map->opts.guard_hi;
    if (size == 0 || size % chunk != 0)
        return NULL;

    if (map->nregions == map->capregions) {
        size_t cap = map->capregions ? map->capregions * 2 : 4;
        struct AddrRegion *regions = realloc(map->regions,
            cap * sizeof(struct AddrRegion));
        if (!regions)
            return NULL;
        map->regions = regions;
        map->capregions = cap;
    }

    size_t want = lo + size + hi;
    if (want < size || want + chunk < want)
        return NULL;

    // Try an exact-size reservation first: it is usable as is if it happens
    // to place the allocatable part on a chunk boundary, which is common when
    // regions are placed next to each other.
    size_t mapsize = want;
    void *p = mmap(NULL, mapsize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1,
        0);
    if (p != MAP_FAILED && ((uintptr_t) p + lo) % chunk != 0) {
        munmap(p, mapsize);
        p = MAP_FAILED;
    }
    if (p == MAP_FAILED) {
        // mmap gives no alignment guarantee, so reserve one extra chunk of
        // slack to find a chunk-aligned start for the allocatable part, then
        // give back whatever lies outside the guards.
        mapsize = want + chunk;
        p = mmap(NULL, mapsize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (p == MAP_FAILED)
            return NULL;
    }
    uintptr_t base = (uintptr_t) p;
    uintptr_t alignbase = ceilp(base + lo, chunk);
    uintptr_t start = alignbase - lo;
    uintptr_t end = alignbase + size + hi;
    assert(start >= base && end <= base + mapsize);
    if (start > base)
        munmap(p, start - base);
    if (end < base + mapsize)
        munmap((void *) end, base + mapsize - end);

    struct ExtAlloc *alloc = extalloc_new(alignbase, size, chunk);
    if (!alloc) {
        munmap((void *) start, end - start);
        return NULL;
    }

    struct AddrRegion *r = &map->regions[map->nregions++];
    *r = (struct AddrRegion) {
        .base = (void *) alignbase,
        .size = size,
        .alloc = alloc,
        .mapbase = (void *) start,
        .mapsize = end - start,
        .ondemand = ondemand,
    };
    return r;
}

bool
boxmap_reserve(struct BoxMap *map, size_t size)
{
    pthread_mutex_lock(&map->lk);
    bool ok = addregion(map, size, false) != NULL;
    pthread_mutex_unlock(&map->lk);
    return ok;
}

uintptr_t
boxmap_addspace(struct BoxMap *map, size_t size)
{
    pthread_mutex_lock(&map->lk);
    uintptr_t p = 0;
    for (size_t i = 0; i < map->nregions; i++) {
        // A region without enough contiguous free space is skipped in favor
        // of the next one.
        p = extalloc_alloc(map->regions[i].alloc, size);
        if (p != 0) {
            map->regions[i].active++;
            goto out;
        }
    }
    // No reserved region has room: reserve a region for just this space.
    struct AddrRegion *r = addregion(map, size, true);
    if (r) {
        p = extalloc_alloc(r->alloc, size);
        assert(p != 0);
        r->active++;
    }
out:
    pthread_mutex_unlock(&map->lk);
    return p;
}

void
boxmap_rmspace(struct BoxMap *map, uintptr_t space, size_t size)
{
    pthread_mutex_lock(&map->lk);
    for (size_t i = 0; i < map->nregions; i++) {
        struct AddrRegion *r = &map->regions[i];
        uintptr_t vabase = (uintptr_t) r->base;
        if (space < vabase || space >= vabase + r->size)
            continue;
        extalloc_free(r->alloc, space, size);
        assert(r->active > 0);
        r->active--;
        if (r->ondemand && r->active == 0) {
            unmapregion(r);
            map->regions[i] = map->regions[--map->nregions];
        }
        break;
    }
    pthread_mutex_unlock(&map->lk);
}
