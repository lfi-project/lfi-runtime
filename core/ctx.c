#include "core.h"
#include "lfi_core.h"

#include <stdlib.h>

extern int
lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr,
    uintptr_t entry) asm("lfi_ctx_entry");

extern void
lfi_ctx_end(struct LFIContext *ctx, int val) asm("lfi_ctx_end");

EXPORT struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp)
{
    struct LFIContext *ctx = malloc(sizeof(struct LFIContext));
    if (!ctx) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    *ctx = (struct LFIContext) {
        .ctxp = ctxp,
        .box = box,
    };

    lfi_ctx_regs_init(ctx);
    ctx->regs.retaddr = box->retaddr;

    return ctx;
}

EXPORT void *
lfi_ctx_data(struct LFIContext *ctx)
{
    return ctx->ctxp;
}

EXPORT int
lfi_ctx_run(struct LFIContext *ctx, uintptr_t entry)
{
    // Save the ctx in invoke info so it can be retrieved via lfi_cur_ctx.
    lfi_invoke_info.ctx = &ctx;
    // Enter the sandbox, saving the stack pointer to host_sp.
    int ret = lfi_ctx_entry(ctx, (uintptr_t *) &ctx->regs.host_sp, entry);
    return ret;
}

EXPORT void
lfi_ctx_free(struct LFIContext *ctx)
{
    free(ctx);
}

EXPORT struct LFIRegs *
lfi_ctx_regs(struct LFIContext *ctx)
{
    return &ctx->regs;
}

EXPORT void
lfi_ctx_exit(struct LFIContext *ctx, int code)
{
    // Exit the sandbox, restoring the stack pointer to the value in host_sp.
    lfi_ctx_end(ctx, code);
}

EXPORT struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx)
{
    return ctx->box;
}

EXPORT struct LFIContext *
lfi_cur_ctx(void)
{
    return *lfi_invoke_info.ctx;
}
