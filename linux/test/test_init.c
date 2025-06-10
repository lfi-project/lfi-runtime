#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

int
main(void)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .no_verify = true,
            .verbose = true,
        },
        gb(256));
    assert(engine);

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .verbose = true,
        });

    lfi_linux_free(linux_);

    lfi_free(engine);

    return 0;
}
