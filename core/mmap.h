#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct MMInfo {
    int prot;
    int flags;
    int fd;
    off_t offset;
    // Was this mapping part of the original address space of this sandbox.
    bool original;
};

struct MMNode {
    uint64_t base;
    size_t len;

    struct MMInfo info;
    struct MMNode *next;
    struct MMNode *prev;
};

struct MMAddrSpace {
    uint64_t base;
    size_t len;
    size_t p2pagesize;

    struct MMNode *nodes;
    struct MMNode *cursor;
};

typedef void (
    *UpdateFn)(uintptr_t start, size_t len, struct MMInfo info, void *udata);

// Note: there is a distinction between 'mappings' and 'regions'. A mapping is
// a set of pages originally created with 'mapany' or 'mapat'. A region is a
// subset of pages from a mapping.

// mm_init initializes the memory mapper for the given virtual address region
// and page size. The page size must be a power of 2.
//
// Returns false if the memory mapper was unable to initialize (no memory).
bool
mm_init(struct MMAddrSpace *mm, uintptr_t start, size_t len, size_t pagesize);

// Deallocates the address space and frees all associated memory.
void
mm_free(struct MMAddrSpace *mm);

// mm_mapany creates a new mapping, where the mapper may choose the location.
//
// The chosen location is returned, or -1 if an error occurred.
uintptr_t
mm_mapany(struct MMAddrSpace *mm, size_t len, int prot, int flags, int fd,
    off_t offset);

// mm_mapat creates a new mapping at a specific address.
//
// The address is returned, or -1 if an error occurred. The range may overlap
// with existing regions, in which case the overlapping portions of the
// existing regions are unmapped and overwritten.
uintptr_t
mm_mapat(struct MMAddrSpace *mm, uintptr_t addr, size_t len, int prot,
    int flags, int fd, off_t offset);

// mm_mapat_cb is the same as mm_mapat but calls 'ufn' when it unmaps regions.
//
// Regions can be unmapped if the requested range overlaps with existing
// regions. In that case the existing portions are unmapped and overwitten.
uintptr_t
mm_mapat_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t len, int prot,
    int flags, int fd, off_t offset, UpdateFn ufn, void *udata);

// mm_unmap unmaps pages from mapped regions, or returns < 0 if an error
// occurred. The range may include unmapped pages.
int
mm_unmap(struct MMAddrSpace *mm, uintptr_t addr, size_t len);

// mm_unmap_cb is the same as mm_unmap but calls 'ufn' when it unmaps regions.
//
// This is useful because unmap may unmap multiple discontiguous regions.
int
mm_unmap_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t len, UpdateFn ufn,
    void *udata);

// mm_query looks up information about a region and places it in 'info'.
//
// Returns false if the requested region is invalid.
bool
mm_querypage(struct MMAddrSpace *mm, uintptr_t addr, struct MMInfo *info);

// mm_protect updates the 'prot' info associated with a region. All pages
// affected by the new protections must be in a mapped region.
//
// Returns 0 on success or a negative error code on failure.
int
mm_protect(struct MMAddrSpace *mm, uintptr_t addr, size_t len, int prot);

// mm_protect_cb is the same as mm_protect but calls 'ufn' for regions that it
// updates.
//
// This is useful since a protection update may affect multiple regions.
int
mm_protect_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t len, int prot,
    UpdateFn ufn, void *udata);

// mm_mark_original marks all current mappings as original.
void
mm_mark_original(struct MMAddrSpace *mm);

// mm_unmap_non_original unmaps all non-original mappings.
void
mm_unmap_non_original(struct MMAddrSpace *mm, UpdateFn ufn, void *udata);
