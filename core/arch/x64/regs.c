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
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
# ifdef GS_CONTEXT
    // The %gs base points to the ctxreg array (set up in lfi_ctx_entry and the
    // trampolines); r15 is a free general-purpose register.
    ctx->regs.r15 = 0;
# else
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
# endif
#endif
#ifdef LARGE_SANDBOX
    ctx->regs.r15 = 0x00000000ffffffff;
#endif
}

EXPORT void
lfi_ctx_regs_relink_ctxreg(struct LFIContext *ctx)
{
#ifdef CTXREG
    ctx->ctxreg[CTXREG_CTX_OFFSET / 8] = (uint64_t) ctx;
# ifndef GS_CONTEXT
    ctx->regs.r15 = (uint64_t) ctx->ctxreg;
# endif
#else
    (void) ctx;
#endif
}
