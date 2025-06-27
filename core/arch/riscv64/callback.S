#include "arch_asm.h"

.text

.macro zero_caller_regs
    #TODO: Implement for RISC-V
.endm

.global lfi_callback
lfi_callback:
    #TODO: Implement RISC-V callback
    ret

.global lfi_callback_userpop
lfi_callback_userpop:
    #TODO: Implement RISC-V userpop
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif