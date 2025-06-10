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

    regs->x0 = syshandle(t, regs->x8, regs->x0, regs->x1, regs->x2, regs->x3,
        regs->x4, regs->x5);
}
