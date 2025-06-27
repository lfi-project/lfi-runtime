#include "arch_asm.h"

.macro zero_regs
    #TODO: Implement RISC-V register zeroing
.endm

.macro get_invoke_info reg
    #TODO: Implement RISC-V TLS access for invoke info
.endm

.global lfi_trampoline
.p2align 4
lfi_trampoline:
    #TODO: Implement RISC-V trampoline
    ret

.global lfi_ret
.p2align 4
lfi_ret:
    #TODO: Implement RISC-V return
    ret

.macro pushx reg
    #TODO: Implement RISC-V register push
.endm

.macro popx reg
    #TODO: Implement RISC-V register pop
.endm

.p2align 4
.trampoline_clone:
    #TODO: Implement RISC-V trampoline clone
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
