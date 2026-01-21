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
#ifdef CTXREG
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
    ctx->ctxreg[0] = (uint64_t) ctx;
#else
    ctx->regs.r15 = 0x00000000ffffffff;
#endif
}
