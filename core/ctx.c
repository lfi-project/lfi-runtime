#include "core.h"
#include "lfi_core.h"

#include <stdlib.h>

_Thread_local struct LFIContext *lfi_ctx;

extern int
lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr,
    uintptr_t entry) asm("lfi_ctx_entry");

extern void
lfi_ctx_end(struct LFIContext *ctx, int val) asm("lfi_ctx_end");

EXPORT struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp)
{
    struct LFIContext *ctx = malloc(sizeof(struct LFIContext));
    if (!ctx) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    *ctx = (struct LFIContext) {
        .ctxp = ctxp,
        .box = box,
    };

    lfi_ctx_regs_init(ctx);

    return ctx;
}

EXPORT int
lfi_ctx_run(struct LFIContext *ctx, uintptr_t entry)
{
    lfi_ctx = ctx;
    // Enter the sandbox, saving the stack pointer to host_sp.
    int ret = lfi_ctx_entry(ctx, &ctx->regs.host_sp, entry);
    return ret;
}

EXPORT void
lfi_ctx_free(struct LFIContext *ctx)
{
    free(ctx);
}

EXPORT struct LFIRegs *
lfi_ctx_regs(struct LFIContext *ctx)
{
    return &ctx->regs;
}

EXPORT void
lfi_ctx_exit(struct LFIContext *ctx, int code)
{
    lfi_ctx = NULL;
    // Exit the sandbox, restoring the stack pointer to the value in host_sp.
    lfi_ctx_end(ctx, code);
}

EXPORT struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx)
{
    return ctx->box;
}

EXPORT void
lfi_ctx_init_ret(struct LFIContext *ctx, lfiptr ret)
{
    ctx->retfn = ret;
}

EXPORT lfiptr
lfi_ctx_ret(struct LFIContext *ctx)
{
    return ctx->retfn;
}
