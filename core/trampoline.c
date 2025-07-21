#include "core.h"

#include <stdlib.h>

EXPORT _Thread_local struct LFIInvokeInfo lfi_invoke_info asm(
    "lfi_invoke_info");

EXPORT extern void
lfi_trampoline() asm("lfi_trampoline");

EXPORT const void *lfi_trampoline_addr = &lfi_trampoline;

EXPORT void
lfi_set_clone_cb(struct LFIEngine *engine,
    struct LFIContext *(*clone_cb_arg)(struct LFIBox *) )
{
    engine->clone_cb = clone_cb_arg;
}

void
lfi_clone(struct LFIBox *box, struct LFIContext **ctxp) asm("lfi_clone");

void
lfi_clone(struct LFIBox *box, struct LFIContext **ctxp)
{
    if (!box->engine->clone_cb) {
        LOG_(
            "error: performing automatic clone during trampoline, but clone_cb is NULL");
        abort();
    }
    struct LFIContext *ctx = box->engine->clone_cb(box);
    *ctxp = ctx;
}
