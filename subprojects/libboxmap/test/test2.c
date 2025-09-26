#include <assert.h>
#include <stdio.h>

#include "boxmap.h"

static size_t gb(size_t x) {
    return x * 1024 * 1024 * 1024;
}

int main(void) {
    struct BoxMap* map = boxmap_new((struct BoxMapOptions) {
        .chunksize = gb(4),
        .guardsize = gb(40),
    });
    assert(map);

    bool ok = boxmap_reserve(map, gb(348));
    assert(ok);
    printf("total size: %ld\n", (long) boxmap_size(map));

    uintptr_t space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);
    space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);
    space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);
    space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);
    space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);
    space = boxmap_addspace(map, gb(44));
    printf("space: %lx\n", (unsigned long) space);
    assert(space != 0);

    boxmap_rmspace(map, space, gb(44));

    boxmap_delete(map);

    return 0;
}
