# libboxmap

A small library for allocating large chunks of the virtual address space.

# API

```c
struct BoxMapOptions {
    // Alignment for allocations inside of regions.
    size_t minalign;
    // Alignment for regions.
    size_t maxalign;
    // Size of space to reserve on either end of a region.
    size_t guardsize;
};

struct BoxMap;

// boxmap_new creates a new BoxMap structure.
struct BoxMap* boxmap_new(struct BoxMapOptions opts);

// boxmap_delete frees a BoxMap structure.
void boxmap_delete(struct BoxMap* map);

// boxmap_size returns the total size available for allocation in 'map'.
uint64_t boxmap_size(struct BoxMap* map);

// boxmap_active returns the number of active spaces in 'map'.
uint64_t boxmap_active(struct BoxMap* map);

// boxmap_reserve reserves 'size' bytes of space in 'map'. If 'size' is 0, it
// will attempt to reserve as much space as possible. Returns false if an error
// occurred.
bool boxmap_reserve(struct BoxMap* map, size_t size);

// boxmap_addspace adds a new space to 'map' of size 'size'. Returns 0 if an
// error occurred.
uintptr_t boxmap_addspace(struct BoxMap* map, size_t size);

// boxmap_rmspace removes the given space from 'map'.
void boxmap_rmspace(struct BoxMap* map, uintptr_t space, size_t size);

// boxmap_strerror returns a string description when a boxmap function returns
// an error.
char* boxmap_strerror(void);
```
