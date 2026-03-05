#include "core.h"
#include "lfi_core.h"

#include <stdlib.h>

extern int
lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr,
    uintptr_t entry) __asm__("lfi_ctx_entry");

extern void
lfi_ctx_end(struct LFIContext *ctx, int val) __asm__("lfi_ctx_end");

EXPORT struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *userdata)
{
    struct LFIContext *ctx = malloc(sizeof(struct LFIContext));
    if (!ctx) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    *ctx = (struct LFIContext) {
        .userdata = userdata,
        .box = box,
    };

    lfi_ctx_regs_init(ctx);

    return ctx;
}

EXPORT void *
lfi_ctx_data(struct LFIContext *ctx)
{
    return ctx->userdata;
}

EXPORT int
lfi_ctx_run(struct LFIContext *ctx, uintptr_t entry)
{
    // Save and restore invoke_info.ctx so that nested calls (e.g. running a
    // signal handler while sandbox execution is still live on this thread)
    // leave the outer ctx intact.
    struct LFIContext **saved_ctxp = lfi_invoke_info.ctx;

    // Save the ctx in invoke info so it can be retrieved via lfi_cur_ctx.
    lfi_invoke_info.ctx = &ctx;
    // Enter the sandbox, saving the stack pointer to host_sp.
    int ret = lfi_ctx_entry(ctx, (uintptr_t *) &ctx->regs.host_sp, entry);

    lfi_invoke_info.ctx = saved_ctxp;
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

EXPORT uint64_t
lfi_ctx_get_tp(struct LFIContext *ctx)
{
#ifdef CTXREG
    return ctx->ctxreg[CTXREG_TP_OFFSET / 8];
#else
    return ctx->regs._tp;
#endif
}

EXPORT void
lfi_ctx_set_tp(struct LFIContext *ctx, uint64_t tp)
{
#ifdef CTXREG
    ctx->ctxreg[CTXREG_TP_OFFSET / 8] = tp;
# if defined(LFI_ARCH_ARM64)
    ctx->regs.x25 = (uint64_t) &ctx->ctxreg[CTXREG_CTX_OFFSET / 8];
# elif defined(LFI_ARCH_X64)
    ctx->regs.r15 = (uint64_t) &ctx->ctxreg[CTXREG_CTX_OFFSET / 8];
# else
# error "CTXREG: architecture not supported"
# endif
#else
    ctx->regs._tp = tp;
#endif
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

EXPORT void
lfi_ctx_abort_callback(struct LFIContext *ctx)
{
    ctx->abort_callback = 1;
}

EXPORT bool
lfi_ctx_abort_status(struct LFIContext *ctx)
{
    return ctx->abort_status == 1;
}
