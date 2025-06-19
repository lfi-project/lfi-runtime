#include "linux.h"

#include "arch_sys.h"
#include "cwalk.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

EXPORT struct LFILinuxEngine *
lfi_linux_new(struct LFIEngine *lfi_engine, struct LFILinuxOptions opts)
{
    if (opts.wd && !cwk_path_is_absolute(opts.wd)) {
        LOG_("wd path '%s' is not absolute", opts.wd);
        return NULL;
    }

    struct LFILinuxEngine *engine = malloc(sizeof(struct LFILinuxEngine));
    if (!engine)
        return NULL;

    const char *verbose = getenv("LFI_VERBOSE");
    if (verbose && strcmp(verbose, "1"))
        opts.verbose = true;

    lfi_sys_handler(lfi_engine, arch_syshandle);

    *engine = (struct LFILinuxEngine) {
        .engine = lfi_engine,
        .opts = opts,
    };

    LOG(engine, "initialized LFI Linux engine");

    return engine;
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

void
lfi_linux_free(struct LFILinuxEngine *engine)
{
    free(engine);
}
