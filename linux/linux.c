#include "linux.h"
#include "proc.h"

#include "arch_sys.h"
#include "cwalk.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#ifndef SYS_MINIMAL
static inline struct sigaction*
get_engine_sigaction(struct LFIContext *ctx, int sig) {
    if (!ctx)
        return NULL;

    struct LFILinuxThread *t = lfi_ctx_data(ctx);
    if (!t || !t->proc || !t->proc->engine)
        return NULL;

    struct LFILinuxEngine *engine = t->proc->engine;
    switch (sig) {
        case LINUX_SIGSEGV: return &engine->oldact_sigsegv;
        case LINUX_SIGILL:  return &engine->oldact_sigill;
        case LINUX_SIGBUS:  return &engine->oldact_sigbus;
        default:
            ERROR("error: unsupported signal (%d) encountered\n", sig);
            __builtin_unreachable();
    }
}

static void
lfi_linux_on_signal(int sig, siginfo_t *si, void *ucontext) {
    // First, try forwarding to sandbox
    // Note: arch_forward_signal MUST be safe to call even if the engine is inconsistent.
    if (lfi_invoke_info.ctx && arch_forward_signal(lfi_cur_ctx(), sig, si, ucontext)) {
        return;
    }

    // Fallback: Sandbox rejected it or isn't running, fetch host handler
    struct sigaction *oldactp = get_engine_sigaction(lfi_cur_ctx(), sig);

    if (!oldactp) {
        struct sigaction dfl_act = { .sa_handler = SIG_DFL };
        sigaction(sig, &dfl_act, NULL);
        return;
    }

    // Invoke the host sigaction
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

// NOTE: is there any need for a per-signal register API?
// NOTE: calling this more than once will clobber the oldact.
static void
lfi_linux_register_sighandler(struct LFILinuxEngine *engine)
{
    struct sigaction act = {0};

    // NOTE: SA_NODEFER to handle a signal raised from within sandbox's
    // sighandler.
    act.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    act.sa_sigaction = lfi_linux_on_signal;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGSEGV, &act, &engine->oldact_sigsegv))
        perror("sigaction");
    if (engine->oldact_sigsegv.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGSEGV is non-default and overwritten\n");

    if (sigaction(SIGILL, &act, &engine->oldact_sigill))
        perror("sigaction");
    if (engine->oldact_sigill.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGILL is non-default and overwritten\n");

    if (sigaction(SIGBUS, &act, &engine->oldact_sigbus))
        perror("sigaction");
    if (engine->oldact_sigbus.sa_handler != SIG_DFL)
        LOG(engine, "warning: existing host handler for SIGBUS is non-default and overwritten\n");
}
#endif

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

#ifndef SYS_MINIMAL
    lfi_linux_register_sighandler(engine);
#endif

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
