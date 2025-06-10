#include "linux.h"

#include "arch_sys.h"
#include "file.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool
init_streams(struct LFILinuxEngine *engine)
{
    engine->fstdin = filefdnew(STDIN_FILENO, LINUX_O_RDONLY);
    if (!engine->fstdin)
        goto err1;
    engine->fstdout = filefdnew(STDOUT_FILENO, LINUX_O_WRONLY);
    if (!engine->fstdout)
        goto err2;
    engine->fstderr = filefdnew(STDERR_FILENO, LINUX_O_WRONLY);
    if (!engine->fstderr)
        goto err3;

    return true;
err3:
    free(engine->fstdout);
err2:
    free(engine->fstdin);
err1:
    return false;
}

EXPORT struct LFILinuxEngine *
lfi_linux_new(struct LFIEngine *lfi_engine, struct LFILinuxOptions opts)
{
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

    if (!init_streams(engine)) {
        free(engine);
        return NULL;
    }

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
    free(engine->fstdout);
    free(engine->fstderr);
    free(engine->fstdin);
    free(engine);
}
