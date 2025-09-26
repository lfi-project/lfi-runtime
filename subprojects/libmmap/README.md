# libmmap

This is a library for implementing memory mapping following the POSIX `mmap`
API. The library provides functionality for defining an address space, and
making map/unmap calls within that address space, although libmmap does not
itself modify virtual memory pages. It is expected that if you are implementing
`mmap`, you will call libmmap to decide where pages should be allocated in the
address space, and then modify the virtual address space accordingly yourself
(either via actual `mmap` calls with `MAP_FIXED` or something custom if you are
writing a kernel).

The libmmap API supports requests to map pages at any location, or at a fixed
location. The library works by maintaining two augmented AVL trees, one for
free regions and one for allocated regions. This makes it easy to support both
allocation anyhwere and allocation at a fixed location, and to support
tracking/updating information about allocated regions, such as their current
protection. Care has also been taken to make sure that the library behaves well
when there isn't enough memory. The state of the AVL trees always remains
consistent even if allocations fail during an operation.

The library also handles the subtleties of overlapping regions for you. Regions
are automatically split, and the user can provide a function to be called when
these updates over multiple regions happen.

# API

```c
typedef struct {
    // base address of the mapping (not the region)
    uint64_t base;
    // length of the mapping
    size_t len;
    int prot;
    int flags;
    int fd;
    off_t offset;
} MMInfo;

typedef void (*UpdateFn)(uint64_t start, size_t len, MMInfo info, void* udata);

// Note: there is a distinction between 'mappings' and 'regions'. A mapping is
// a set of pages originally created with 'mapany' or 'mapat'. A region is a
// subset of pages from a mapping.

// mm_init initializes the memory mapper for the given virtual address region
// and page size. The page size must be a power of 2.
//
// Returns false if the memory mapper was unable to initialize (no memory).
bool mm_init(MMAddrSpace* mm, uint64_t start, size_t len, size_t pagesize);

// mm_mapany creates a new mapping, where the mapper may choose the location.
//
// The chosen location is returned, or -1 if an error occurred.
uint64_t mm_mapany(MMAddrSpace* mm, size_t len, int prot, int flags, int fd, off_t offset);

// mm_mapat creates a new mapping at a specific address.
//
// The address is returned, or -1 if an error occurred. The range may overlap
// with existing regions, in which case the overlapping portions of the
// existing regions are unmapped and overwritten.
uint64_t mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset);

// mm_mapat_cb is the same as mm_mapat but calls 'ufn' when it unmaps regions.
//
// Regions can be unmapped if the requested range overlaps with existing
// regions. In that case the existing portions are unmapped and overwitten.
uint64_t mm_mapat_cb(MMAddrSpace* mm, uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset, UpdateFn ufn, void* udata);

// mm_unmap unmaps pages from mapped regions, or returns < 0 if an error
// occurred. The range may include unmapped pages.
int mm_unmap(MMAddrSpace* mm, uint64_t addr, size_t len);

// mm_unmap_cb is the same as mm_unmap but calls 'ufn' when it unmaps regions.
//
// This is useful because unmap may unmap multiple discontiguous regions.
int mm_unmap_cb(MMAddrSpace* mm, uint64_t addr, size_t len, UpdateFn ufn, void* udata);

// mm_query looks up information about a region and places it in 'info'.
//
// Returns false if the requested region is invalid.
bool mm_querypage(MMAddrSpace* mm, uint64_t addr, MMInfo* info);

// mm_protect updates the 'prot' info associated with a region. All pages
// affected by the new protections must be in a mapped region.
//
// Returns 0 on success or a negative error code on failure.
int mm_protect(MMAddrSpace* mm, uint64_t addr, size_t len, int prot);

// mm_protect_cb is the same as mm_protect but calls 'ufn' for regions that it
// updates.
//
// This is useful since a protection update may affect multiple regions.
int mm_protect_cb(MMAddrSpace* mm, uint64_t addr, size_t len, int prot, UpdateFn ufn, void* udata);
```
