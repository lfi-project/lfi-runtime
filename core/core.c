#include "core.h"

#include <assert.h>
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
    if (opts->verbose)
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
# ifdef __GLIBC__
    // With glibc we need to make sure GLIBC_TUNABLES=glibc.pthread.rseq=0. See
    // https://issues.chromium.org/issues/428179540 and
    // https://lore.kernel.org/all/cover.1747817128.git.dvyukov@google.com/ for
    // details.
    char *tunables = getenv("GLIBC_TUNABLES");
    if (!tunables || strstr(tunables, "pthread.rseq=0") == NULL) {
        ERROR("error: missing GLIBC_TUNABLES=glibc.pthread.rseq=0 environment variable");
        return false;
    }
# endif
    return true;
}
#endif

static bool
init_sigaltstack(struct LFIEngine *engine)
{
    engine->altstack.ss_sp = malloc(SIGSTKSZ);
    if (engine->altstack.ss_sp == NULL) {
        lfi_error = LFI_ERR_ALLOC;
        return false;
    }
    engine->altstack.ss_size = SIGSTKSZ;
    engine->altstack.ss_flags = 0;
    if (sigaltstack(&engine->altstack, NULL) == -1) {
        free(engine->altstack.ss_sp);
        lfi_error = LFI_ERR_SIGALTSTACK;
        return false;
    }
    return true;
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
        .chunksize = gb(4),
        // This is the guard size between the edge of the boxmap region and the
        // outer world.
        .guardsize = box_footprint(gb(4), opts),
    };
    struct BoxMap *bm = boxmap_new(bm_opts);
    if (!bm) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }

    // Reserve space for n sandboxes with appropriate footprint, 2 guard pages,
    // and 2 chunks worth of slack because boxmap has to do internal alignment
    // when mmap returns non-chunk-aligned region.
    size_t reserve = nsandboxes * box_footprint(opts.boxsize, opts) +
        bm_opts.chunksize * 2 + bm_opts.guardsize * 2;

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
        .guardsize = kb(80),
    };

    if (!engine->opts.no_init_sigaltstack)
        if (!init_sigaltstack(engine))
            goto err2;

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
lfi_syscall_handler(struct LFIContext *ctx) asm("lfi_syscall_handler");

void
lfi_syscall_handler(struct LFIContext *ctx)
{
    assert(ctx->box->engine->sys_handler &&
        "engine does not have a system call handler");
    ctx->box->engine->sys_handler(ctx);
}
