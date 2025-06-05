#include "core.h"

_Thread_local lfiptr lfi_retfn;
_Thread_local lfiptr lfi_targetfn;

extern void
lfi_trampoline() asm("lfi_trampoline");

const void *lfi_trampoline_addr = &lfi_trampoline;
