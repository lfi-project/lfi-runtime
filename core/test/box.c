#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static bool
isbyte(char *buf, size_t size, char b)
{
    for (size_t i = 0; i < size; i++)
        if (buf[i] != b)
            return false;
    return true;
}

int
main(void)
{
    size_t pagesize = getpagesize();
    // Initialize LFI.
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .noverify = true,
        },
        gb(256));

    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    // Create a new sandbox.
    struct LFIBox *box = lfi_box_new(engine);
    if (!box) {
        fprintf(stderr, "failed to create LFI sandbox: %s\n", lfi_errmsg());
        exit(1);
    }

    // Get sandbox information.
    struct LFIBoxInfo info = lfi_box_info(box);
    assert(info.size == gb(4));

    // Request a mapping from anywhere in the sandbox (should succeed).
    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ, LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    assert(lfi_box_ptrvalid(box, p));

    // Check that the data is all zero.
    char buf[8];
    lfi_box_copyfm(box, buf, p, sizeof(buf));
    assert(isbyte(buf, sizeof(buf), 0));

    // Change the mapping to read/write.
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_WRITE);
    assert(r == 0);

    // Write all 42s to the first 8 bytes of the mapping.
    memset(buf, 42, sizeof(buf));
    lfi_box_copyto(box, p, buf, sizeof(buf));

    // Check that the write happened by directly accessing the memory.
    assert(isbyte((char *) lfi_box_l2p(box, p), sizeof(buf), 42));

    // Try to make a RWX page (should fail).
    r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_WRITE | LFI_PROT_EXEC);
    assert(r == -1);

    lfi_free(engine);

    return 0;
}
