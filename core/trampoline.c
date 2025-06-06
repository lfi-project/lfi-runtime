#include "core.h"

EXPORT _Thread_local lfiptr lfi_retfn;
EXPORT _Thread_local lfiptr lfi_targetfn;

EXPORT extern void
lfi_trampoline() asm("lfi_trampoline");

EXPORT const void *lfi_trampoline_addr = &lfi_trampoline;
