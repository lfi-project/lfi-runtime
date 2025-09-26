#include <assert.h>

#include "mmap.h"
#include "test.h"

int main(void) {
    struct MMAddrSpace mm;
    bool ok = mm_init(&mm, 0, PAGESIZE * 16, PAGESIZE);
    assert(ok);

    uint64_t r;
    r = mm_mapat(&mm, 3 * PAGESIZE, 8 * PAGESIZE, 0, 0, 0, 0);
    assert(r != (uintptr_t) -1);
    r = mm_unmap(&mm, r, 8 * PAGESIZE);
    assert(r == 0);
    r = mm_mapany(&mm, 16 * PAGESIZE, 0, 0, 0, 0);
    assert(r != (uintptr_t) -1);

    mm_free(&mm);

    return 0;
}
