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

    regs->a0 = syshandle(t, regs->a7, regs->a0, regs->a1, regs->a2, regs->a3,
        regs->a4, regs->a5);
}