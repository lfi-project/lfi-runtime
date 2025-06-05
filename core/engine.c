#include "core.h"

#include <assert.h>
#include <stdlib.h>

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
#else
#error "invalid architecture: must be one of arm64, x64"
#endif
}

EXPORT struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t reserve)
{
    struct LFIEngine *engine = malloc(sizeof(struct LFIEngine));
    if (!engine) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    struct BoxMap *bm = boxmap_new((struct BoxMapOptions) {
        .minalign = gb(4),
        .maxalign = gb(4),
        .guardsize = kb(80),
    });
    if (!bm) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }
    if (!boxmap_reserve(bm, reserve)) {
        lfi_error = LFI_ERR_RESERVE;
        lfi_error_desc = xasprintf("attempted to reserve %ld bytes", reserve);
        goto err2;
    }

    *engine = (struct LFIEngine) {
        .bm = bm,
        .opts = opts,
        .guardsize = kb(80),
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

void
lfi_free(struct LFIEngine *engine)
{
    // Unmaps all virtual memory reserved by the engine.
    boxmap_delete(engine->bm);
    free(engine);
}

// Declare this function with asm ("lfi_syscall_handler") so that it will be
// callable from hand-writtem assembly (runtime.S). Otherwise, macOS names the
// symbol _lfi_syscall_handler.
void
lfi_syscall_handler(struct LFIContext *ctx) asm("lfi_syscall_handler");

void
lfi_syscall_handler(struct LFIContext *ctx)
{
    assert(ctx->box->engine->opts.sys_handler &&
        "engine does not have a system call handler");
    ctx->box->engine->opts.sys_handler(ctx);
}
