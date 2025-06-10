#include "sys.h"

#include "arch_sys.h"
#include "lfi_arch.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>

void
arch_syshandle(struct LFIContext *ctx)
{
    struct LFILinuxThread *t = lfi_ctx_data(ctx);
    assert(t);
    struct LFIRegs *regs = lfi_ctx_regs(ctx);

    uint64_t orig_rax = regs->rax;

    regs->rax = syshandle(t, orig_rax, regs->rdi, regs->rsi, regs->rdx,
        regs->r10, regs->r8, regs->r9);
}
