#include "lfi_core.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

    lfi_free(engine);

    return 0;
}
