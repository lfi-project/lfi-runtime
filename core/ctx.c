#include "core.h"
#include "ctxreg.h"
#include "lfi_core.h"

#include <stdlib.h>
#include <sys/mman.h>

extern int
lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr,
    uintptr_t entry) __asm__("lfi_ctx_entry");

extern void
lfi_ctx_end(struct LFIContext *ctx, int val) __asm__("lfi_ctx_end");

EXPORT struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *userdata)
{
    pku_host_access();

    struct LFIContext *ctx = malloc(sizeof(struct LFIContext));
    if (!ctx) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    // The ctxreg array lives in a two-page block: a read-only runtime call
    // page followed by the read-write ctxreg page holding the array.
    size_t pagesize = box->engine->opts.pagesize;
    void *block = mmap(NULL, CTX_BLOCK_SIZE(pagesize), PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) {
        free(ctx);
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    if (!ctx_block_init(block, pagesize, box->pkey, ctx)) {
        munmap(block, CTX_BLOCK_SIZE(pagesize));
        free(ctx);
        lfi_error = LFI_ERR_MMAP;
        return NULL;
    }

    *ctx = (struct LFIContext) {
        .ctxreg = (uint64_t *) ((char *) block +
            CTX_BLOCK_CTXREG_OFF(pagesize)),
        .block = block,
        .userdata = userdata,
        .box = box,
    };

#ifdef SEGUE_CACHE_GS
    ctx->gs_cache = &lfi_invoke_info.gs_base;
#endif

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
    lfi_init_sigaltstack(ctx->box->engine);
    // Save the ctx in invoke info so it can be retrieved via lfi_cur_ctx.
    lfi_invoke_info.ctx = &ctx;
#ifdef SEGUE_CACHE_GS
    // Bind the context's %gs cache slot to this (the running) thread; the
    // lfi_ctx_entry path does not go through the trampoline.
    ctx->gs_cache = &lfi_invoke_info.gs_base;
#endif
    // Enter the sandbox, saving the stack pointer to host_sp.
    ctx->in_ctx_run = true;
    int ret = lfi_ctx_entry(ctx, (uintptr_t *) &ctx->regs.host_sp, entry);
    ctx->in_ctx_run = false;
    return ret;
}

EXPORT void
lfi_ctx_free(struct LFIContext *ctx)
{
    if (!ctx)
        return;
    munmap(ctx->block, CTX_BLOCK_SIZE(ctx->box->engine->opts.pagesize));
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
    return ctx->ctxreg[CTXREG_TP_OFFSET / 8];
}

EXPORT void
lfi_ctx_set_tp(struct LFIContext *ctx, uint64_t tp)
{
    ctx->ctxreg[CTXREG_TP_OFFSET / 8] = tp;
# if defined(LFI_ARCH_ARM64)
    ctx->regs.x25 = (uint64_t) &ctx->ctxreg[0];
# elif defined(LFI_ARCH_X64)
    ctx->regs.r15 = (uint64_t) &ctx->ctxreg[0];
# elif defined(LFI_ARCH_RISCV64)
    ctx->regs.s10 = (uint64_t) &ctx->ctxreg[0];
# else
# error "CTXREG: architecture not supported"
# endif
}

EXPORT void
lfi_ctx_exit(struct LFIContext *ctx, int code)
{
    // Exit the sandbox, restoring the stack pointer to the value in host_sp.
    if (!ctx->in_ctx_run)
        abort();
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
lfi_ctx_abort_cb(struct LFIContext *ctx)
{
    ctx->abort_callback = 1;
}

EXPORT bool
lfi_ctx_abort_cb_status(struct LFIContext *ctx)
{
    return ctx->abort_status == 1;
}
