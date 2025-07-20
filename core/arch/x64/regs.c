#include "arch_asm.h"
#include "core.h"

EXPORT void
lfi_ctx_regs_init(struct LFIContext *ctx)
{
    ctx->regs.REG_BASE = ctx->box->base;
    ctx->regs.rsp = ctx->box->base;
    ctx->regs.retaddr = ctx->box->retaddr;
    ctx->regs.pkey = ctx->box->pkey;
}
