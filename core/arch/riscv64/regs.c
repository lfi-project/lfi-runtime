#include "arch_asm.h"
#include "core.h"

EXPORT void
lfi_ctx_regs_init(struct LFIContext *ctx)
{
    ctx->regs.x18 = ctx->box->base;
    ctx->regs.x21 = ctx->box->base;
}
