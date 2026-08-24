#include "arch_asm.h"
#include "core.h"

EXPORT void
lfi_ctx_regs_init(struct LFIContext *ctx)
{
    ctx->regs.REG_BASE = ctx->box->base;
    ctx->regs.rsp = ctx->box->base;
    ctx->regs.retaddr = ctx->box->retaddr;
    ctx->regs.pkey = ctx->box->pkey;
    ctx->regs.fcw = 0x37f;
    ctx->regs.mxcsr = 0x1f80;
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
}

EXPORT void
lfi_ctx_regs_relink_ctxreg(struct LFIContext *ctx)
{
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
}
