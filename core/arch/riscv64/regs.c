#include "arch_asm.h"
#include "core.h"

EXPORT void
lfi_ctx_regs_init(struct LFIContext *ctx)
{
    ctx->regs.REG_BASE = ctx->box->base;
    ctx->regs.REG_ADDR = ctx->box->base;
    ctx->regs.sp = ctx->box->base;
    ctx->regs.ra = ctx->box->base;
    ctx->regs.retaddr = ctx->box->retaddr;
    ctx->regs.REG_CTX = (uint64_t) &ctx->ctxreg[0];
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
}

EXPORT void
lfi_ctx_regs_relink_ctxreg(struct LFIContext *ctx)
{
    ctx->regs.REG_CTX = (uint64_t) &ctx->ctxreg[0];
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
}
