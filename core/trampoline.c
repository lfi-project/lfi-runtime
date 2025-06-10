#include "core.h"

EXPORT _Thread_local struct LFIInvokeInfo lfi_invoke_info;

EXPORT extern void
lfi_trampoline() asm("lfi_trampoline");

EXPORT const void *lfi_trampoline_addr = &lfi_trampoline;
