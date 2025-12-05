#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    size_t pagesize = getpagesize();

    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
        },
        1);

    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    struct LFIBox *box = lfi_box_new(engine);
    if (!box) {
        fprintf(stderr, "failed to create LFI sandbox: %s\n", lfi_errmsg());
        exit(1);
    }

    // Test 1: Basic mquery on a new mapping
    lfiptr p1 = lfi_box_mapany(box, pagesize, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p1 != (lfiptr) -1);

    struct LFIMapInfo info;
    bool found = lfi_box_mquery(box, p1, &info);
    assert(found);
    assert(info.prot == LFI_PROT_READ);
    assert(info.flags == (LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE));
    assert(info.fd == -1);
    assert(info.offset == 0);

    // Test 2: mquery on unmapped address returns false
    struct LFIBoxInfo boxinfo = lfi_box_info(box);
    found = lfi_box_mquery(box, boxinfo.max - pagesize, &info);
    assert(!found);

    // Test 3: mprotect updates the info
    int r = lfi_box_mprotect(box, p1, pagesize, LFI_PROT_READ | LFI_PROT_WRITE);
    assert(r == 0);

    found = lfi_box_mquery(box, p1, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    // Test 4: Multiple mappings with different protections
    lfiptr p2 = lfi_box_mapany(box, pagesize * 2, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p2 != (lfiptr) -1);

    found = lfi_box_mquery(box, p2, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    // Query second page of the mapping
    found = lfi_box_mquery(box, p2 + pagesize, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    // Test 5: mprotect on partial range splits the mapping
    r = lfi_box_mprotect(box, p2, pagesize, LFI_PROT_READ);
    assert(r == 0);

    // First page should now be read-only
    found = lfi_box_mquery(box, p2, &info);
    assert(found);
    assert(info.prot == LFI_PROT_READ);

    // Second page should still be read-write
    found = lfi_box_mquery(box, p2 + pagesize, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    // Test 6: mprotect with gaps (unmapped pages in range)
    lfiptr p3 = lfi_box_mapany(box, pagesize, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p3 != (lfiptr) -1);

    // Unmap p3 to create a gap, then do mprotect over a range that includes it
    r = lfi_box_munmap(box, p3, pagesize);
    assert(r == 0);

    // Verify p3 is unmapped
    found = lfi_box_mquery(box, p3, &info);
    assert(!found);

    // Test 7: munmap and verify query fails
    r = lfi_box_munmap(box, p1, pagesize);
    assert(r == 0);

    found = lfi_box_mquery(box, p1, &info);
    assert(!found);

    // Test 8: mapat at specific address
    lfiptr p4 = lfi_box_mapat(box, p1, pagesize, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE | LFI_MAP_FIXED, -1, 0);
    assert(p4 == p1);

    found = lfi_box_mquery(box, p4, &info);
    assert(found);
    assert(info.prot == LFI_PROT_READ);

    lfi_box_free(box);
    lfi_free(engine);

    return 0;
}
