#include "lfi_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline size_t
gb(size_t x)
{
    return x * 1024 * 1024 * 1024;
}

int
main(void)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .noverify = true,
        },
        gb(256));

    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    return 0;
}
