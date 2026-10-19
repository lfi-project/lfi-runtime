#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <errno.h>
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

    // Test 9: mprotect_mapped refuses ranges with gaps or reservations
    lfiptr p5 = lfi_box_mapany(box, pagesize * 3, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p5 != (lfiptr) -1);
    r = lfi_box_mprotect_mapped(box, p5, pagesize * 3,
        LFI_PROT_READ | LFI_PROT_WRITE);
    assert(r == 0);
    found = lfi_box_mquery(box, p5 + 2 * pagesize, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    r = lfi_box_munmap(box, p5 + pagesize, pagesize);
    assert(r == 0);
    r = lfi_box_mprotect_mapped(box, p5, pagesize * 3, LFI_PROT_READ);
    assert(r == -ENOMEM);
    // Nothing changed, including the mapped pages around the gap.
    found = lfi_box_mquery(box, p5, &info);
    assert(found);
    assert(info.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

    lfiptr p6 = lfi_box_mapat(box, p5 + pagesize, pagesize, LFI_PROT_NONE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE | LFI_MAP_RESERVE, -1, 0);
    assert(p6 == p5 + pagesize);
    r = lfi_box_mprotect_mapped(box, p5, pagesize * 3, LFI_PROT_READ);
    assert(r == -ENOMEM);
    r = lfi_box_mprotect_mapped(box, p5, pagesize, LFI_PROT_READ);
    assert(r == 0);
    r = lfi_box_mprotect_mapped(box, p5 + 1, pagesize, LFI_PROT_READ);
    assert(r == -EINVAL);

    // Test 10: mapat_noreplace replaces reservations and free pages, but not
    // mappings. p5 is now: mapped, reserved, mapped.
    lfiptr q = lfi_box_mapat_noreplace(box, p5, pagesize * 3, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(q == (lfiptr) -1);
    found = lfi_box_mquery(box, p5 + pagesize, &info);
    assert(found);
    assert(info.flags & LFI_MAP_RESERVE);
    q = lfi_box_mapat_noreplace(box, p5 + pagesize, pagesize,
        LFI_PROT_READ | LFI_PROT_WRITE, LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1,
        0);
    assert(q == p5 + pagesize);
    found = lfi_box_mquery(box, q, &info);
    assert(found);
    assert(!(info.flags & LFI_MAP_RESERVE));
    r = lfi_box_munmap(box, p5, pagesize * 3);
    assert(r == 0);
    q = lfi_box_mapat_noreplace(box, p5, pagesize * 3, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(q == p5);

    // Test 11: the sandbox-facing calls refuse pinned mappings, but the plain
    // calls the runtime uses still work on them.
    lfiptr pin = lfi_box_mapany(box, pagesize * 2, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE | LFI_MAP_PINNED, -1, 0);
    assert(pin != (lfiptr) -1);
    r = lfi_box_mprotect_mapped(box, pin + pagesize, pagesize,
        LFI_PROT_READ | LFI_PROT_WRITE);
    assert(r == -EACCES);
    r = lfi_box_munmap_unpinned(box, pin, pagesize);
    assert(r == -EINVAL);
    q = lfi_box_mapat_unpinned(box, pin + pagesize, pagesize,
        LFI_PROT_READ | LFI_PROT_WRITE, LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1,
        0);
    assert(q == (lfiptr) -1);
    q = lfi_box_mapat_noreplace(box, pin, pagesize, LFI_PROT_READ,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(q == (lfiptr) -1);
    found = lfi_box_mquery(box, pin + pagesize, &info);
    assert(found);
    assert(info.prot == LFI_PROT_READ);
    assert(info.flags & LFI_MAP_PINNED);
    // Unpinned ranges next to it are unaffected.
    r = lfi_box_munmap_unpinned(box, p5, pagesize * 3);
    assert(r == 0);
    r = lfi_box_munmap(box, pin, pagesize * 2);
    assert(r == 0);
    found = lfi_box_mquery(box, pin, &info);
    assert(!found);

    // Test 12: copyto_checked writes only to writable, mapped pages, including
    // when the destination straddles two pages.
    lfiptr w = lfi_box_mapany(box, pagesize * 2, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(w != (lfiptr) -1);
    uint64_t val = 0x1122334455667788;
    bool ok = lfi_box_copyto_checked(box, w + pagesize - 4, &val, sizeof(val));
    assert(ok);
    uint64_t back;
    lfi_box_copyfm(box, &back, w + pagesize - 4, sizeof(back));
    assert(back == val);
    r = lfi_box_mprotect(box, w + pagesize, pagesize, LFI_PROT_READ);
    assert(r == 0);
    ok = lfi_box_copyto_checked(box, w + pagesize - 4, &val, sizeof(val));
    assert(!ok);
    r = lfi_box_munmap(box, w + pagesize, pagesize);
    assert(r == 0);
    ok = lfi_box_copyto_checked(box, w + pagesize - 4, &val, sizeof(val));
    assert(!ok);
    q = lfi_box_mapat(box, w + pagesize, pagesize,
        LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE | LFI_MAP_RESERVE, -1, 0);
    assert(q == w + pagesize);
    ok = lfi_box_copyto_checked(box, w + pagesize - 4, &val, sizeof(val));
    assert(!ok);
    ok = lfi_box_copyto_checked(box, w, &val, sizeof(val));
    assert(ok);
    r = lfi_box_munmap(box, w, pagesize * 2);
    assert(r == 0);

    lfi_box_free(box);
    lfi_free(engine);

    return 0;
}
