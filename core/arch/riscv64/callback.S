#include "arch_asm.h"

.text

.macro zero_caller_regs
    #TODO: Implement for RISC-V
.endm

.global lfi_callback
lfi_callback:
    #TODO: Implement RISC-V callback
    ebreak

.global lfi_callback_userpop

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif