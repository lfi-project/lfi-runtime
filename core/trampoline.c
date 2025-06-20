#include "core.h"

EXPORT _Thread_local struct LFIInvokeInfo lfi_invoke_info asm("lfi_invoke_info");

EXPORT extern void
lfi_trampoline() asm("lfi_trampoline");

EXPORT const void *lfi_trampoline_addr = &lfi_trampoline;

// TODO: this is global state that is kind of ugly. Either LFI_INVOKE should
// take an engine as well, or we should commit to having one global LFI engine.
static _Thread_local struct LFIContext *(*clone_cb)(void);

EXPORT void
lfi_set_clone_cb(struct LFIContext *(*clone_cb_arg)(void))
{
    clone_cb = clone_cb_arg;
}

void
lfi_clone(struct LFIContext **ctxp)
{
    *ctxp = clone_cb();
}
