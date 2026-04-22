#include "linux.h"

#include "arch_sys.h"
#include "cwalk.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

struct sigaction oldact_sigsegv;
struct sigaction oldact_sigill;
struct sigaction oldact_sigbus;

static void
lfi_linux_on_signal(int sig, siginfo_t *si, void *ucontext) {
    struct sigaction *oldactp;

    switch (sig) {
        case LINUX_SIGSEGV:
            oldactp = &oldact_sigsegv;
            break;
        case LINUX_SIGILL:
            oldactp = &oldact_sigill;
            break;
        case LINUX_SIGBUS:
            oldactp = &oldact_sigbus;
            break;
        default:
            ERROR("signal handling error: unsupported signal (%d) encountered\n", sig);
            __builtin_unreachable();
    }

    if (!arch_forward_signal(lfi_cur_ctx(), sig, si, ucontext)) {
        if (oldactp->sa_flags & SA_SIGINFO) {
            oldactp->sa_sigaction(sig, si, ucontext);
        } else if (oldactp->sa_handler == SIG_DFL || oldactp->sa_handler == SIG_IGN) {
            sigaction(sig, oldactp, NULL);
            // sandbox will execute the faulting instruction again.
            // NOTE: will not be true for asynchronous signals
        } else {
            oldactp->sa_handler(sig);
        }
    }
}

// NOTE: is there any need for a per-signal register API?
// NOTE: calling this more than once will clobber the oldact.
static void
lfi_linux_register_sighandler(struct LFILinuxEngine *engine)
{
    struct sigaction act;

    // NOTE: Do I need SA_NODEFER ?
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    act.sa_sigaction = lfi_linux_on_signal;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGSEGV, &act, &oldact_sigsegv))
        perror("sigaction");
    if (oldact_sigsegv.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGSEGV is non-default and overwritten\n");

    if (sigaction(SIGILL, &act, &oldact_sigill))
        perror("sigaction");
    if (oldact_sigill.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGILL is non-default and overwritten\n");

    if (sigaction(SIGBUS, &act, &oldact_sigbus))
        perror("sigaction");
    if (oldact_sigill.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGBUS is non-default and overwritten\n");
}

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

    lfi_linux_register_sighandler(engine);

    LOG(engine, "initialized LFI Linux engine");

    return engine;
}

// Global engine for libraries.
static struct LFILinuxEngine *lib_engine;

// Total number of procs (libraries) that the lib_engine is initialized for.
#define MAX_LIBRARIES 1

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
