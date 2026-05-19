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
#ifdef LARGE_SANDBOX
    ctx->regs.REG_MASK = ctx->box->size - 1;
    ctx->regs._tp = 0;
#else
#ifdef CTXREG
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
#else
    ctx->regs.r15 = 0x00000000ffffffff;
#endif
#endif
}

EXPORT void
lfi_ctx_thread_regs_init(struct LFIContext* ctx)
{
#if defined(LARGE_SANDBOX) && defined(SINGLE_SANDBOX)
    __asm__ __volatile__("wrgsbase %0" : : "r"(ctx->regs._tp));
#endif
}
