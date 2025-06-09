#include "linux.h"

#include <stdlib.h>
#include <unistd.h>

EXPORT struct LFILinuxEngine *
lfi_linux_new(struct LFIEngine *engine, struct LFILinuxOptions opts)
{
    struct LFILinuxEngine *lengine = malloc(sizeof(struct LFILinuxEngine));
    if (!lengine)
        return NULL;
    *lengine = (struct LFILinuxEngine) {
        .engine = engine,
        .opts = opts,
    };

    return lengine;
}

static struct LFILinuxEngine *lib_engine;

EXPORT bool
lfi_linux_lib_init(void *base, void *end, void *entry, void *phdrs,
    struct LFIOptions opts, struct LFILinuxOptions linux_opts)
{
    if (!lib_engine) {
        struct LFIEngine *engine = lfi_new(opts, 0);
        if (!engine) {
            LOG_("fatal error initializing LFI: %s\n", lfi_errmsg());
            return false;
        }
        lib_engine = lfi_linux_new(engine, linux_opts);
        if (!lib_engine) {
            LOG_("fatal error initializing LFI linux\n");
            return false;
        }
    }

    // TODO:
    // Initialize a context/thread/proc with a stack and stuff
    // run the entrypoint

    return true;
}
