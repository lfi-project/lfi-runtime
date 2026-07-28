#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NALLOC 8

static struct LFIEngine *
newengine(bool no_aslr)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .no_aslr = no_aslr,
        },
        2);
    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }
    return engine;
}

// Map NALLOC regions of increasing size and record each offset from the start
// of the sandbox.
static void
alloc_offsets(struct LFIBox *box, lfiptr offsets[NALLOC])
{
    size_t pagesize = getpagesize();
    struct LFIBoxInfo info = lfi_box_info(box);
    for (size_t i = 0; i < NALLOC; i++) {
        size_t size = pagesize * (i + 1);
        lfiptr p = lfi_box_mapany(box, size, LFI_PROT_READ | LFI_PROT_WRITE,
            LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
        assert(p != (lfiptr) -1);
        assert(p % pagesize == 0);
        assert(lfi_box_bufvalid(box, p, size));

        // The mapping must be usable.
        lfi_box_copyto(box, p, &i, sizeof(i));

        struct LFIMapInfo minfo;
        bool found = lfi_box_mquery(box, p, &minfo);
        assert(found);
        assert(minfo.prot == (LFI_PROT_READ | LFI_PROT_WRITE));

        offsets[i] = p - info.min;
    }
}

int
main(void)
{
    size_t pagesize = getpagesize();

    // Two boxes with ASLR (the default) should get different layouts.
    struct LFIEngine *engine = newengine(false);
    struct LFIBox *box1 = lfi_box_new(engine);
    assert(box1);
    struct LFIBox *box2 = lfi_box_new(engine);
    assert(box2);

    lfiptr offsets1[NALLOC], offsets2[NALLOC];
    alloc_offsets(box1, offsets1);
    alloc_offsets(box2, offsets2);
    assert(memcmp(offsets1, offsets2, sizeof(offsets1)) != 0);

    // Unmapping the randomly placed regions must return their space: after
    // box1 is emptied, a maximum-size mapping must succeed.
    struct LFIBoxInfo info = lfi_box_info(box1);
    for (size_t i = 0; i < NALLOC; i++) {
        int r = lfi_box_munmap(box1, info.min + offsets1[i],
            pagesize * (i + 1));
        assert(r == 0);
    }
    lfiptr big = lfi_box_mapany(box1, info.max - info.min, LFI_PROT_NONE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(big != (lfiptr) -1);
    assert(big == info.min);

    lfi_box_free(box1);
    lfi_box_free(box2);
    lfi_free(engine);

    // With no_aslr, two boxes should get identical (deterministic) layouts.
    engine = newengine(true);
    box1 = lfi_box_new(engine);
    assert(box1);
    box2 = lfi_box_new(engine);
    assert(box2);

    lfiptr det1[NALLOC], det2[NALLOC];
    alloc_offsets(box1, det1);
    alloc_offsets(box2, det2);
    assert(memcmp(det1, det2, sizeof(det1)) == 0);

    // The randomized layouts should differ from the deterministic one.
    assert(memcmp(offsets1, det1, sizeof(det1)) != 0);
    assert(memcmp(offsets2, det1, sizeof(det1)) != 0);

    lfi_box_free(box1);
    lfi_box_free(box2);
    lfi_free(engine);

    return 0;
}
