#include "core.h"
#include "lfi_core.h"

#include <stdlib.h>

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

    // TODO: initialize registers

    return ctx;
}

EXPORT int
lfi_ctx_run(struct LFIContext *ctx)
{

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
}

EXPORT struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx)
{
    return ctx->box;
}
