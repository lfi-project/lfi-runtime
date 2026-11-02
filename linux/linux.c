#include "linux.h"

#include "arch_sys.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

EXPORT struct LFILinuxEngine *
lfi_linux_new(struct LFIEngine *lfi_engine, struct LFILinuxOptions opts)
{
#if !defined(LINUX_FULL) || !defined(__linux__)
    if (opts.sys_passthrough) {
        ERROR("sys_passthrough requires the full configuration on Linux "
              "(-Dlinux_full=true)");
        return NULL;
    }
#endif

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

// Number of procs (libraries) that the lib_engine reserves address space for
// up front. More can be created; each reserves its own space on demand.
#define PRERESERVED_LIBRARIES 2

EXPORT bool
lfi_linux_lib_init(struct LFIOptions opts, struct LFILinuxOptions linux_opts)
{
    if (!lib_engine) {
        struct LFIEngine *engine = lfi_new(opts, PRERESERVED_LIBRARIES);
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

EXPORT void
lfi_linux_lib_free(void)
{
    if (lib_engine) {
        lfi_free(lib_engine->engine);
        free(lib_engine);
        lib_engine = NULL;
    }
}

EXPORT void
lfi_linux_free(struct LFILinuxEngine *engine)
{
    free(engine);
}
