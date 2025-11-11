#define _GNU_SOURCE

#include "core.h"
#include "lfi_arch.h"

#include <signal.h>

void
trapsys_handler(int signum, siginfo_t *info, void *ucontext)
{
    struct LFIContext *ctx = *lfi_invoke_info.ctx;

    ucontext_t *uc = (ucontext_t *) ucontext;
    mcontext_t *mctx = &uc->uc_mcontext;

    ctx->regs.rsp = mctx->gregs[REG_RSP];
    ctx->regs.rbp = mctx->gregs[REG_RBP];
    ctx->regs.rax = mctx->gregs[REG_RAX];
    ctx->regs.rbx = mctx->gregs[REG_RBX];
    ctx->regs.rcx = mctx->gregs[REG_RCX];
    ctx->regs.rdx = mctx->gregs[REG_RDX];
    ctx->regs.rsi = mctx->gregs[REG_RSI];
    ctx->regs.rdi = mctx->gregs[REG_RDI];
    ctx->regs.r8 = mctx->gregs[REG_R8];
    ctx->regs.r9 = mctx->gregs[REG_R9];
    ctx->regs.r10 = mctx->gregs[REG_R10];
    ctx->regs.r11 = mctx->gregs[REG_R11];
    ctx->regs.r12 = mctx->gregs[REG_R12];
    ctx->regs.r13 = mctx->gregs[REG_R13];
    ctx->regs.r14 = mctx->gregs[REG_R14];
    ctx->regs.r15 = mctx->gregs[REG_R15];

    ctx->box->engine->sys_handler(ctx);

    mctx->gregs[REG_RSP] = ctx->regs.rsp;
    mctx->gregs[REG_RBP] = ctx->regs.rbp;
    mctx->gregs[REG_RAX] = ctx->regs.rax;
    mctx->gregs[REG_RBX] = ctx->regs.rbx;
    mctx->gregs[REG_RCX] = ctx->regs.rcx;
    mctx->gregs[REG_RDX] = ctx->regs.rdx;
    mctx->gregs[REG_RSI] = ctx->regs.rsi;
    mctx->gregs[REG_RDI] = ctx->regs.rdi;
    mctx->gregs[REG_R8] = ctx->regs.r8;
    mctx->gregs[REG_R9] = ctx->regs.r9;
    mctx->gregs[REG_R10] = ctx->regs.r10;
    mctx->gregs[REG_R11] = ctx->regs.r11;
    mctx->gregs[REG_R12] = ctx->regs.r12;
    mctx->gregs[REG_R13] = ctx->regs.r13;
    mctx->gregs[REG_R14] = ctx->regs.r14;
    mctx->gregs[REG_R15] = ctx->regs.r15;
}
