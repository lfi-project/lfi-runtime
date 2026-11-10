#include "core.h"

#include <stdlib.h>

EXPORT thread_local struct LFIInvokeInfo lfi_invoke_info __asm__(
    "lfi_invoke_info");

EXPORT void
lfi_set_invoke_info(struct LFIContext **ctx, lfiptr targetfn,
    struct LFIBox *box)
{
    lfi_invoke_info.ctx = ctx;
    lfi_invoke_info.targetfn = targetfn;
    lfi_invoke_info.box = box;
}

EXPORT void
lfi_set_clone_cb(struct LFIEngine *engine,
    struct LFIContext *(*clone_cb_arg)(struct LFIBox *) )
{
    __atomic_store_n(&engine->clone_cb, clone_cb_arg, __ATOMIC_RELEASE);
}

void
lfi_clone(struct LFIBox *box, struct LFIContext **ctxp) __asm__("lfi_clone");

EXPORT void
lfi_clone(struct LFIBox *box, struct LFIContext **ctxp)
{
    lfi_init_sigaltstack(box->engine);
    struct LFIContext *(*clone_cb)(struct LFIBox *) = __atomic_load_n(
        &box->engine->clone_cb, __ATOMIC_ACQUIRE);
    if (!clone_cb) {
        LOG_(
            "error: performing automatic clone during trampoline, but clone_cb is NULL");
        abort();
    }
    // This host thread is attaching to the sandbox for the first time.
    pku_host_access();
    struct LFIContext *ctx = clone_cb(box);
    if (!ctx) {
        LOG_("error: clone callback did not produce a context");
        abort();
    }
    *ctxp = ctx;
}
