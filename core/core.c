#include "core.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static void
logerr(char *msg, size_t size)
{
    (void) size;
    LOG_("%s", msg);
}

static void
init_verifier(struct LFIVerifier *v, struct LFIOptions *opts)
{
    if (opts->no_verify)
        return;

    if (opts->stores_only)
        v->opts.box = LFI_BOX_STORES;
    else
        v->opts.box = LFI_BOX_FULL;

    v->opts.ctxreg = true;
    v->opts.no_base_rtcalls = opts->ctxreg_rtcalls;
    v->opts.err = logerr;

#if defined(LFI_ARCH_ARM64)
    v->verify = lfiv_verify_arm64;
#elif defined(LFI_ARCH_X64)
    v->verify = lfiv_verify_x64;
#elif defined(LFI_ARCH_RISCV64)
    v->verify = lfiv_verify_riscv64;
#else
#error "invalid architecture"
#endif
}

#ifdef HAVE_PKU
#include <features.h>
static bool
check_pku(void)
{
#ifdef __GLIBC__
    // With glibc we need to make sure GLIBC_TUNABLES=glibc.pthread.rseq=0. See
    // https://issues.chromium.org/issues/428179540 and
    // https://lore.kernel.org/all/cover.1747817128.git.dvyukov@google.com/ for
    // details.
    char *tunables = getenv("GLIBC_TUNABLES");
    if (!tunables || strstr(tunables, "pthread.rseq=0") == NULL) {
        ERROR(
            "error: missing GLIBC_TUNABLES=glibc.pthread.rseq=0 environment variable");
        lfi_error = LFI_ERR_PKU;
        return false;
    }
#endif
    return true;
}
#endif

static bool
has_sigaltstack(void)
{
    stack_t cur;
    if (sigaltstack(NULL, &cur) == -1)
        return false;
    return (cur.ss_flags & SS_DISABLE) == 0 && cur.ss_sp != NULL;
}

static void
disable_sigaltstack(void)
{
    sigaltstack(&(stack_t) { .ss_flags = SS_DISABLE }, NULL);
}

// Everything a thread that has entered a sandbox has to release when it exits.
// The state lives in TLS and the key exists only so that the destructor runs.
struct ThreadState {
    void *altstack;
    void (*cleanup)(void *data);
    void *cleanup_data;
};

static thread_local struct ThreadState tstate;
static thread_local bool tstate_armed;

static pthread_key_t tstate_key;
static pthread_once_t tstate_key_once = PTHREAD_ONCE_INIT;
static bool tstate_key_ready;

static void
tstate_destructor(void *p)
{
    struct ThreadState *st = p;

    tstate_armed = false;

    // Must come before freeing the sigaltstack since the cleanup may run code
    // in the sandbox.
    if (st->cleanup) {
        void (*cleanup)(void *) = st->cleanup;
        void *data = st->cleanup_data;
        st->cleanup = NULL;
        st->cleanup_data = NULL;
        cleanup(data);
    }

    if (st->altstack) {
        disable_sigaltstack();
        free(st->altstack);
        st->altstack = NULL;
    }
}

static void
init_tstate_key(void)
{
    tstate_key_ready = pthread_key_create(&tstate_key, tstate_destructor) == 0;
}

// Arranges for this thread's state to be torn down when it exits.
static bool
arm_tstate(void)
{
    if (tstate_armed)
        return true;
    pthread_once(&tstate_key_once, init_tstate_key);
    if (!tstate_key_ready || pthread_setspecific(tstate_key, &tstate) != 0)
        return false;
    tstate_armed = true;
    return true;
}

EXPORT bool
lfi_set_thread_cleanup(void (*cleanup)(void *data), void *data)
{
    if (!arm_tstate())
        return false;
    tstate.cleanup = cleanup;
    tstate.cleanup_data = data;
    return true;
}

EXPORT void *
lfi_thread_cleanup_data(void)
{
    return tstate.cleanup_data;
}

// Set once this thread has been through lfi_init_sigaltstack.
static thread_local bool altstack_done;

void
lfi_init_sigaltstack(struct LFIEngine *engine)
{
    if (engine->opts.no_init_sigaltstack || altstack_done)
        return;
    altstack_done = true;

    if (has_sigaltstack()) {
        LOG(engine, "alternate signal stack already registered");
        return;
    }

    if (!arm_tstate()) {
        lfi_error = LFI_ERR_SIGALTSTACK;
        ERROR("warning: failed to register thread state destructor");
        return;
    }

    stack_t ss = (stack_t) {
        .ss_sp = malloc(SIGSTKSZ),
        .ss_size = SIGSTKSZ,
        .ss_flags = 0,
    };
    if (ss.ss_sp == NULL) {
        lfi_error = LFI_ERR_ALLOC;
        ERROR("warning: failed to allocate signal stack");
        return;
    }
    if (sigaltstack(&ss, NULL) == -1) {
        free(ss.ss_sp);
        lfi_error = LFI_ERR_SIGALTSTACK;
        ERROR("warning: failed to register signal stack");
        return;
    }
    tstate.altstack = ss.ss_sp;
}

EXPORT struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t nsandboxes)
{
#ifdef HAVE_PKU
    if (!check_pku())
        return NULL;
#endif
    // The usable range of a sandbox ends a whole number of pages before its
    // end, leaving room for the sys page of a sandbox placed above it.
    if (opts.pagesize == 0 || BOX_INTERNAL_GUARD < opts.pagesize ||
        BOX_INTERNAL_GUARD % opts.pagesize != 0) {
        lfi_error = LFI_ERR_UNSUPPORTED;
        lfi_error_desc = xasprintf("page size %zu does not divide the %zu-byte "
                                   "internal sandbox guard",
            opts.pagesize, (size_t) BOX_INTERNAL_GUARD);
        return NULL;
    }
#if defined(LFI_ARCH_RISCV64)
    if (opts.ctxreg_rtcalls) {
        lfi_error = LFI_ERR_UNSUPPORTED;
        lfi_error_desc = xasprintf("ctxreg_rtcalls is not supported on riscv64");
        return NULL;
    }
#endif

    struct LFIEngine *engine = malloc(sizeof(struct LFIEngine));
    if (!engine) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    // Every region keeps REGION_GUARD below its first sandbox, and, unless
    // sandboxes have no sys page (ctxreg_rtcalls), at least a page so that the
    // first sandbox's sys page is inside the region. Above the last sandbox,
    // the sandbox's own external guard already provides BOX_EXTERNAL_GUARD of
    // the region guard.
    size_t guard_lo = REGION_GUARD;
    if (!opts.ctxreg_rtcalls && guard_lo < opts.pagesize)
        guard_lo = opts.pagesize;
    size_t guard_hi = REGION_GUARD > BOX_EXTERNAL_GUARD ?
        REGION_GUARD - BOX_EXTERNAL_GUARD :
        0;
    struct BoxMapOptions bm_opts = (struct BoxMapOptions) {
        .chunksize = gb(4),
        .guard_lo = guard_lo,
        .guard_hi = guard_hi,
    };
    struct BoxMap *bm = boxmap_new(bm_opts);
    if (!bm) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }

    // Pre-reserve space for n sandboxes in one region. Sandboxes beyond these
    // get a region of their own when they are created.
    size_t reserve = nsandboxes * box_footprint(opts.boxsize);
    if (nsandboxes > 0) {
        if (reserve / nsandboxes != box_footprint(opts.boxsize) ||
            !boxmap_reserve(bm, reserve)) {
            lfi_error = LFI_ERR_RESERVE;
            lfi_error_desc = xasprintf("attempted to reserve %zu bytes",
                reserve + guard_lo + guard_hi);
            goto err2;
        }
    }

    const char *verbose = getenv("LFI_VERBOSE");
    if (verbose && strcmp(verbose, "1") == 0)
        opts.verbose = true;

    *engine = (struct LFIEngine) {
        .bm = bm,
        .opts = opts,
    };

    if (opts.no_verify)
        LOG(engine, "unsafe: verification disabled");
    if (opts.allow_wx)
        LOG(engine, "unsafe: allowing WX pages");

    init_verifier(&engine->verifier, &opts);

    LOG(engine, "initialized LFI engine: %zu GiB reserved for %zu sandboxes",
        (reserve + (nsandboxes > 0 ? guard_lo + guard_hi : 0)) / gb(1),
        nsandboxes);

    return engine;

err2:
    boxmap_delete(bm);
err1:
    free(engine);
    return NULL;
}

EXPORT void
lfi_sys_handler(struct LFIEngine *engine,
    void (*sys_handler)(struct LFIContext *ctx))
{
    engine->sys_handler = sys_handler;
}

EXPORT void
lfi_free(struct LFIEngine *engine)
{
    // Unmaps all virtual memory reserved by the engine.
    boxmap_delete(engine->bm);
    free(engine);
}

EXPORT struct LFIOptions
lfi_opts(struct LFIEngine *engine)
{
    return engine->opts;
}

// Declare this function with asm ("lfi_syscall_handler") so that it will be
// callable from hand-writtem assembly (runtime.S). Otherwise, macOS names the
// symbol _lfi_syscall_handler.
void
lfi_syscall_handler(struct LFIContext *ctx) __asm__("lfi_syscall_handler");

void
lfi_syscall_handler(struct LFIContext *ctx)
{
    assert(ctx->box->engine->sys_handler &&
        "engine does not have a system call handler");
    ctx->box->engine->sys_handler(ctx);
}
