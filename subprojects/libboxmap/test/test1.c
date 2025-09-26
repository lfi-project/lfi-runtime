#include <assert.h>
#include <stdio.h>

#include "boxmap.h"

static size_t gb(size_t x) {
    return x * 1024 * 1024 * 1024;
}

int main(void) {
    struct BoxMap* map = boxmap_new((struct BoxMapOptions) {
        .chunksize = gb(4),
        .guardsize = gb(4),
    });
    assert(map);

    bool ok = boxmap_reserve(map, gb(32));
    assert(ok);
    printf("total size: %ld\n", (long) boxmap_size(map));

    uintptr_t space = boxmap_addspace(map, gb(4));
    printf("space: %lx\n", space);
    boxmap_rmspace(map, space, gb(4));
    space = boxmap_addspace(map, gb(4));
    printf("space: %lx\n", space);
    boxmap_rmspace(map, space, gb(4));

    boxmap_delete(map);

    return 0;
}
