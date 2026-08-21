#include "core.h"

#include <assert.h>
#include <limits.h>
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

#ifdef STORES_ONLY
    v->opts.box = LFI_BOX_STORES;
#else
    v->opts.box = LFI_BOX_FULL;
#endif

#ifdef CTXREG
    v->opts.ctxreg = true;
#endif
    v->opts.err = logerr;

#ifdef LARGE_SANDBOX
    v->opts.large = true;
    v->opts.p2size = LARGE_SANDBOX_BITS;
#endif

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

// Holds the stack a thread installed for itself so that the destructor can
// free it when that thread exits.
static pthread_key_t altstack_key;
static pthread_once_t altstack_key_once = PTHREAD_ONCE_INIT;
static bool altstack_key_ready;

// Set once this thread has been through lfi_init_sigaltstack.
static thread_local bool altstack_done;

// Number of destructor rounds this thread has been through.
static thread_local int altstack_rounds;

static void
altstack_destructor(void *stack)
{
    // Destructors run in rounds, in an unspecified order within a round, and
    // some of them re-enter the sandbox on this thread (the Linux runtime
    // tears down a thread's context with a sandbox call). Handing the stack
    // back to the key asks for another round, which keeps it registered until
    // every destructor that does not do the same has finished.
    if (++altstack_rounds < PTHREAD_DESTRUCTOR_ITERATIONS &&
        pthread_setspecific(altstack_key, stack) == 0)
        return;
    disable_sigaltstack();
    free(stack);
}

static void
init_altstack_key(void)
{
    altstack_key_ready = pthread_key_create(&altstack_key,
        altstack_destructor) == 0;
}

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

    pthread_once(&altstack_key_once, init_altstack_key);
    if (!altstack_key_ready) {
        lfi_error = LFI_ERR_SIGALTSTACK;
        ERROR("warning: failed to create signal stack key");
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

    if (pthread_setspecific(altstack_key, ss.ss_sp) != 0) {
        disable_sigaltstack();
        free(ss.ss_sp);
        lfi_error = LFI_ERR_SIGALTSTACK;
        ERROR("warning: failed to register signal stack destructor");
    }
}

EXPORT struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t nsandboxes)
{
#ifdef HAVE_PKU
    if (!check_pku())
        return NULL;
#endif

    struct LFIEngine *engine = malloc(sizeof(struct LFIEngine));
    if (!engine) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    struct BoxMapOptions bm_opts = (struct BoxMapOptions) {
        .chunksize = opts.boxsize,
        .guardsize = REGION_GUARD,
    };
    struct BoxMap *bm = boxmap_new(bm_opts);
    if (!bm) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }

    // Reserve space for n sandboxes with appropriate footprint, region guards
    // on each side, and 1 chunk worth of slack because boxmap has to do
    // internal alignment when mmap returns non-chunk-aligned region.
    size_t reserve = nsandboxes * box_footprint(opts.boxsize) +
        bm_opts.chunksize + bm_opts.guardsize * 2;

    if (nsandboxes > 0) {
        if (!boxmap_reserve(bm, reserve)) {
            lfi_error = LFI_ERR_RESERVE;
            lfi_error_desc = xasprintf("attempted to reserve %ld bytes",
                reserve);
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

    LOG(engine, "initialized LFI engine: %ld GiB",
        reserve / 1024 / 1024 / 1024);

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
