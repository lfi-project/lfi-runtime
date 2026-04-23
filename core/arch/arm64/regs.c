#include "arch_asm.h"
#include "core.h"

EXPORT void
lfi_ctx_regs_init(struct LFIContext *ctx)
{
    ctx->regs.REG_BASE = ctx->box->base;
    ctx->regs.REG_ADDR = ctx->box->base;
    ctx->regs.sp = ctx->box->base;
    ctx->regs.x30 = ctx->box->base;
    ctx->regs.retaddr = ctx->box->retaddr;
#ifdef CTXREG
    ctx->regs.x25 = (uint64_t) &ctx->ctxreg[CTXREG_CTX_OFFSET / 8];
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
#endif
}

EXPORT void
lfi_ctx_thread_regs_init(struct LFIContext* ctx)
{
}
