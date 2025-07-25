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
        ERROR("wd path '%s' is not absolute", opts.wd);
        return NULL;
    }

    struct LFILinuxEngine *engine = malloc(sizeof(struct LFILinuxEngine));
    if (!engine)
        return NULL;

    const char *verbose = getenv("LFI_VERBOSE");
    if (verbose && strcmp(verbose, "1") == 0)
        opts.verbose = true;
    const char *debug = getenv("LFI_DEBUG");
    if (debug && strcmp(debug, "1") == 0)
        opts.debug = true;

    lfi_sys_handler(lfi_engine, arch_syshandle);

    *engine = (struct LFILinuxEngine) {
        .engine = lfi_engine,
        .opts = opts,
    };

    LOG(engine, "initialized LFI Linux engine");

    return engine;
}

// Global engine for libraries.
static struct LFILinuxEngine *lib_engine;

// Total number of procs (libraries) that the lib_engine is initialized for.
#define MAX_LIBRARIES 16

EXPORT bool
lfi_linux_lib_init(struct LFIOptions opts, struct LFILinuxOptions linux_opts)
{
    if (!lib_engine) {
        struct LFIEngine *engine = lfi_new(opts, MAX_LIBRARIES);
        if (!engine) {
            ERROR("fatal error initializing LFI: %s\n", lfi_errmsg());
            return false;
        }
        lib_engine = lfi_linux_new(engine, linux_opts);
        if (!lib_engine) {
            ERROR("fatal error initializing LFI linux\n");
            return false;
        }
    }

    return true;
}

EXPORT struct LFILinuxEngine *
lfi_linux_lib_engine(void)
{
    return lib_engine;
}

void
lfi_linux_free(struct LFILinuxEngine *engine)
{
    free(engine);
}
