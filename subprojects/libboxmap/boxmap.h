#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct BoxMapOptions {
    // Size in bytes of the minimum allocatable chunk.
    size_t chunksize;
    // Size in bytes to reserve on either end of a region. Must be zero or a
    // multiple of the chunk size.
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
