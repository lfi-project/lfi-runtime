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
#ifdef ENABLE_SW_SHSTK
    // Reserved data stack pointer in the dual-stack scheme. The thread
    // initializer overrides this with the real stack pointer; defaulting to
    // the sandbox base keeps the register in-sandbox before then.
    ctx->regs.r13 = ctx->box->base;
#endif
#ifdef CTXREG
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
#else
    ctx->regs.r15 = 0x00000000ffffffff;
#endif
}
