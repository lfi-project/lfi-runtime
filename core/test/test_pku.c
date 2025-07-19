#ifndef HAVE_PKU
#include <stdio.h>
int
main(void)
{
    printf("compiled without PKU support\n");
    return 0;
}

#else

#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
            .use_pku = true,
        },
        1);

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

    lfi_box_free(box);

    lfi_free(engine);

    return 0;
}

#endif
